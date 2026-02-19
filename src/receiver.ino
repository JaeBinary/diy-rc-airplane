// ========================================
// RC Glider Receiver with NRF24L01
// ========================================
// 기능: 송신기로부터 RF 신호를 받아 ESC와 서보 제어
// - ESC: 모터 속도 제어 (1510~2000us)
// - Aileron: 좌우 제어 서보
// - Elevator: 상하 제어 서보
// - 중립 위치 2초 유지 시 서보 자동 detach (전력 절약)
// - 서보 천천히 움직이기 (전류 급증 방지)
// ========================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

// ========================================
// 디버그 모드 설정
// ========================================
#define DEBUG_MODE 0  // 1: 시리얼 출력 활성화, 0: 비활성화

// ========================================
// 핀 설정
// ========================================
#define CE_PIN 2
#define CSN_PIN 3
#define MOTOR_ESC_PIN 8
#define SERVO_AILERON_PIN 9
#define SERVO_ELEVATOR_PIN 10

// ========================================
// 서보 각도 범위
// ========================================
#define SERVO_MIN 30   // 최소 각도
#define SERVO_MAX 150  // 최대 각도

// ========================================
// ESC 범위 (측정값 기반)
// ========================================
#define ESC_STOP 1500  // 정지
#define ESC_MIN 1510   // 최소 작동값 (측정 완료)
#define ESC_MAX 2000   // 최대 속도

// ========================================
// RF 통신 설정
// ========================================
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "JBKPJ";  // 통신 주소 (송신기와 동일해야 함)

// ========================================
// 서보 객체
// ========================================
Servo motorESC;
Servo servoAileron;
Servo servoElevator;

// ========================================
// 통신 데이터 구조체
// ========================================
struct Signal {
  byte throttle;  // 스로틀 (0~255)
  byte aileron;   // 에일러론 (0~255, 127=중립)
  byte elevator;  // 엘리베이터 (0~255, 127=중립)
  byte rudder;    // 러더 (미사용)
};

Signal data;

// ========================================
// 서보 스무딩 변수
// ========================================
float smoothed_aileron = 90.0;
float smoothed_elevator = 90.0;
const float SMOOTHING = 0.2;  // 스무딩 계수 (0.0~1.0, 클수록 빠름)

int last_aileron_output = 90;
int last_elevator_output = 90;

// ========================================
// 서보 현재 위치 (천천히 움직이기용)
// ========================================
int current_aileron_pos = 90;
int current_elevator_pos = 90;
unsigned long lastServoUpdate = 0;
const unsigned long SERVO_UPDATE_INTERVAL = 20;  // 20ms마다 서보 업데이트

// ========================================
// 서보 자동 detach 타이머
// ========================================
unsigned long aileron_neutral_time = 0;
unsigned long elevator_neutral_time = 0;
bool aileron_attached = true;
bool elevator_attached = true;

// ========================================
// 데이터 초기화 (신호 끊김 시 안전 값)
// ========================================
void ResetData() {
  data.throttle = 0;     // 모터 정지
  data.aileron = 127;    // 중립
  data.elevator = 127;   // 중립
  data.rudder = 127;     // 중립
}

// ========================================
// 데드밴드 적용 (미세한 떨림 방지)
// ========================================
// 현재값과 목표값의 차이가 deadband 이하면 현재값 유지
// 중립 근처(85~95도)는 더 큰 데드밴드 적용
int applyDeadband(int current, int target, int deadband = 3) {
  int diff = abs(target - current);
  
  // 중립 근처는 데드밴드 확대
  if (target >= 85 && target <= 95) {
    deadband = 5;
  }
  
  if (diff <= deadband) {
    return current;
  }
  return target;
}

// ========================================
// 통신 상태 체크 변수
// ========================================
unsigned long lastRecvTime = 0;

#if DEBUG_MODE
  int receiveCount = 0;
  unsigned long lastPrintTime = 0;
#endif

// ========================================
// 초기화
// ========================================
void setup() {
  #if DEBUG_MODE
    Serial.begin(9600);
    Serial.println("=== RC Glider Receiver ===");
  #endif
  
  // ESC 초기화 (먼저 attach하고 정지 신호 전송)
  motorESC.attach(MOTOR_ESC_PIN);
  motorESC.writeMicroseconds(ESC_STOP);
  
  // 서보 초기화
  servoAileron.attach(SERVO_AILERON_PIN);
  servoElevator.attach(SERVO_ELEVATOR_PIN);

  // ESC 초기화 대기 (삐삑 소리 확인)
  delay(2000);

  // RF 모듈 초기화
  radio.begin();
  radio.setChannel(108);                // 108번 채널
  radio.setDataRate(RF24_250KBPS);      // 250kbps
  radio.setPALevel(RF24_PA_MAX);        // 최대 출력
  radio.openReadingPipe(0, address);    // 수신 파이프 열기
  radio.startListening();               // 수신 모드 시작

  // 데이터 초기화
  ResetData();
  
  // 서보 중립 위치
  servoAileron.write(90);
  servoElevator.write(90);
  
  // 현재 위치 초기화
  current_aileron_pos = 90;
  current_elevator_pos = 90;
  
  #if DEBUG_MODE
    Serial.println("ESC: 1510~2000us");
    Serial.println("Servo: Slow move enabled");
    Serial.println("Deadband: 3° (neutral 5°)");
    Serial.println("Auto-detach: 2s at neutral");
    Serial.println("Ready!");
    Serial.println();
  #endif
}

// ========================================
// 메인 루프
// ========================================
void loop() {
  // RF 신호 수신
  if (radio.available()) {
    radio.read(&data, sizeof(Signal));
    lastRecvTime = millis();
    
    #if DEBUG_MODE
      receiveCount++;
    #endif
  }

  unsigned long now = millis();
  unsigned long timeSinceLastRecv = now - lastRecvTime;
  
  // 신호 끊김 감지 (1초 이상 수신 없음)
  if (timeSinceLastRecv > 1000) {
    ResetData();
  }

  // ========================================
  // 모터 제어
  // ========================================
  int motor_pulse;
  if (data.throttle == 0) {
    motor_pulse = ESC_STOP;  // 완전 정지
  } else {
    // 1~255를 1510~2000us로 매핑
    motor_pulse = map(data.throttle, 1, 255, ESC_MIN, ESC_MAX);
  }
  motorESC.writeMicroseconds(motor_pulse);
  
  // ========================================
  // 서보 제어 (스무딩 적용)
  // ========================================
  int aileron_target = map(data.aileron, 0, 255, SERVO_MIN, SERVO_MAX);
  int elevator_target = map(data.elevator, 0, 255, SERVO_MIN, SERVO_MAX);
  
  // 스무딩 필터 적용
  smoothed_aileron = smoothed_aileron * (1.0 - SMOOTHING) + aileron_target * SMOOTHING;
  smoothed_elevator = smoothed_elevator * (1.0 - SMOOTHING) + elevator_target * SMOOTHING;
  
  // 데드밴드 적용
  int aileron_output = applyDeadband(last_aileron_output, (int)smoothed_aileron, 3);
  int elevator_output = applyDeadband(last_elevator_output, (int)smoothed_elevator, 3);
  
  // ========================================
  // Aileron 중립 체크 (전력 절약)
  // ========================================
  if (aileron_output >= 85 && aileron_output <= 95) {
    // 중립 위치
    if (aileron_neutral_time == 0) {
      aileron_neutral_time = now;  // 중립 시작 시간 기록
    } else if (now - aileron_neutral_time > 2000 && aileron_attached) {
      // 2초 이상 중립 → detach
      servoAileron.detach();
      aileron_attached = false;
      #if DEBUG_MODE
        Serial.println("Aileron OFF");
      #endif
    }
  } else {
    // 중립 벗어남 → 다시 attach
    if (!aileron_attached) {
      servoAileron.attach(SERVO_AILERON_PIN);
      aileron_attached = true;
      #if DEBUG_MODE
        Serial.println("Aileron ON");
      #endif
    }
    aileron_neutral_time = 0;
  }
  
  // ========================================
  // Elevator 중립 체크 (전력 절약)
  // ========================================
  if (elevator_output >= 80 && elevator_output <= 100) {
    // 중립 위치
    if (elevator_neutral_time == 0) {
      elevator_neutral_time = now;
    } else if (now - elevator_neutral_time > 2000 && elevator_attached) {
      // 2초 이상 중립 → detach
      servoElevator.detach();
      elevator_attached = false;
      #if DEBUG_MODE
        Serial.println("Elevator OFF");
      #endif
    }
  } else {
    // 중립 벗어남 → 다시 attach
    if (!elevator_attached) {
      servoElevator.attach(SERVO_ELEVATOR_PIN);
      elevator_attached = true;
      #if DEBUG_MODE
        Serial.println("Elevator ON");
      #endif
    }
    elevator_neutral_time = 0;
  }
  
  // ========================================
  // 서보 출력 (천천히 움직이기)
  // ========================================
  // 20ms마다 서보 업데이트 (전류 급증 완화)
  if (now - lastServoUpdate >= SERVO_UPDATE_INTERVAL) {
    lastServoUpdate = now;
    
    // Aileron: 천천히 목표 위치로 이동 (1도씩)
    if (aileron_attached) {
      if (current_aileron_pos < aileron_output) {
        current_aileron_pos = min(current_aileron_pos + 1, aileron_output);
      } else if (current_aileron_pos > aileron_output) {
        current_aileron_pos = max(current_aileron_pos - 1, aileron_output);
      }
      servoAileron.write(current_aileron_pos);
    }
    
    // Elevator: 천천히 목표 위치로 이동 (1도씩)
    if (elevator_attached) {
      if (current_elevator_pos < elevator_output) {
        current_elevator_pos = min(current_elevator_pos + 1, elevator_output);
      } else if (current_elevator_pos > elevator_output) {
        current_elevator_pos = max(current_elevator_pos - 1, elevator_output);
      }
      servoElevator.write(current_elevator_pos);
    }
  }
  
  // 이전 값 저장
  last_aileron_output = aileron_output;
  last_elevator_output = elevator_output;
  
  // ========================================
  // 디버그 출력 (1초마다)
  // ========================================
  #if DEBUG_MODE
    if (now - lastPrintTime >= 1000) {
      if (timeSinceLastRecv < 1000) {
        Serial.print("RX:");
        Serial.print(receiveCount);
        Serial.print(" | T:");
        Serial.print(data.throttle);
        Serial.print("(");
        Serial.print(motor_pulse);
        Serial.print("us) | A:");
        Serial.print(current_aileron_pos);  // 현재 위치 출력
        Serial.print("°→");
        Serial.print(aileron_output);  // 목표 위치 출력
        Serial.print(" ");
        Serial.print(aileron_attached ? "ON" : "OFF");
        Serial.print(" | E:");
        Serial.print(current_elevator_pos);  // 현재 위치 출력
        Serial.print("°→");
        Serial.print(elevator_output);  // 목표 위치 출력
        Serial.print(" ");
        Serial.println(elevator_attached ? "ON" : "OFF");
      } else {
        Serial.println("SIGNAL LOST!");
      }
      
      lastPrintTime = now;
      receiveCount = 0;
    }
  #endif
}
