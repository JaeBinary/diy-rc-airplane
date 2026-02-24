// ========================================
// RC Glider Transmitter with NRF24L01
// ========================================
// 기능: 조이스틱 입력을 읽어 RF로 수신기에 전송
// - 조이스틱 2개 (Throttle, Aileron/Elevator)
// - NRF24L01 RF 모듈 사용
// - 신호 전송 빈도: 50Hz (20ms)
// ========================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ========================================
// 디버그 모드 설정
// ========================================
#define DEBUG_MODE 1  // 1: 시리얼 출력 활성화, 0: 비활성화

// ========================================
// 핀 설정
// ========================================
#define CE_PIN 2
#define CSN_PIN 3

// 조이스틱 핀 (아날로그 입력)
#define JOYSTICK_THROTTLE_PIN A1  // 조이스틱 1 Y축 (위아래 → 모터 속도)
#define JOYSTICK_AILERON_PIN A0   // 조이스틱 2 X축 (좌우 → 에일러론)
#define JOYSTICK_ELEVATOR_PIN A2  // 조이스틱 2 Y축 (위아래 → 엘리베이터)
#define JOYSTICK_RUDDER_PIN A3    // 미사용

// ========================================
// 조이스틱 캘리브레이션 값
// ========================================
// 조이스틱 중립값 (실측 후 조정 필요)
#define JOY_CENTER_MIN 480
#define JOY_CENTER_MAX 540
#define JOY_MIN 0
#define JOY_MAX 1023

// ========================================
// RF 통신 설정
// ========================================
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "JBKPJ";  // 수신기와 동일한 주소

// ========================================
// 통신 데이터 구조체
// ========================================
struct Signal {
  byte throttle;  // 0~255
  byte aileron;   // 0~255 (127=중립)
  byte elevator;  // 0~255 (127=중립)
  byte rudder;    // 0~255 (127=중립, 미사용)
};

Signal data;

// ========================================
// 전송 타이밍 제어
// ========================================
unsigned long lastTransmitTime = 0;
const unsigned long TRANSMIT_INTERVAL = 20;  // 20ms = 50Hz

#if DEBUG_MODE
  int transmitCount = 0;
  int failCount = 0;
  unsigned long lastPrintTime = 0;
  bool transmitSuccess = true;
#endif

// ========================================
// RF 모듈 초기화 함수
// ========================================
bool initRadio() {
  // 전원 안정화 대기
  delay(100);
  
  // CE, CSN 핀 초기화
  pinMode(CE_PIN, OUTPUT);
  pinMode(CSN_PIN, OUTPUT);
  digitalWrite(CE_PIN, LOW);
  digitalWrite(CSN_PIN, HIGH);
  delay(5);
  
  // RF 모듈 초기화
  if (!radio.begin()) {
    return false;
  }
  
  // 수신부와 동일한 설정
  radio.setChannel(108);                // 108번 채널
  radio.setDataRate(RF24_250KBPS);      // 250kbps
  radio.setPALevel(RF24_PA_MAX);        // 최대 출력
  
  // Auto-ACK 및 재전송 설정
  radio.setAutoAck(true);
  radio.setRetries(5, 15);              // 재전송 설정
  
  radio.openWritingPipe(address);       // 송신 파이프 열기
  radio.stopListening();                // 송신 모드
  
  // 설정 안정화 대기
  delay(50);
  
  return true;
}

// ========================================
// 조이스틱 값 읽기 및 매핑
// ========================================
byte mapThrottle(int rawValue) {
  // Throttle: 위로 올릴수록(0에 가까울수록) 높은 출력
  // rawValue: 0(위) ~ 1023(아래)
  // output: 255(위/최대) ~ 0(아래/정지)
  
  // 값 반전: 1023 - rawValue
  int inverted = JOY_MAX - rawValue;
  
  // 중립 이하는 0으로 처리
  if (inverted < JOY_CENTER_MAX) {
    return 0;
  }
  
  // 중립 이상만 0~255로 매핑
  return map(inverted, JOY_CENTER_MAX, JOY_MAX, 0, 255);
}

byte mapElevator(int rawValue) {
  // Elevator: 위(0) → 상승, 아래(1023) → 하강
  // rawValue: 0(위) ~ 1023(아래)
  // output: 255(위/상승) ~ 0(아래/하강)
  
  // 값 반전
  int inverted = JOY_MAX - rawValue;
  
  // 데드존 적용
  if (inverted >= JOY_CENTER_MIN && inverted <= JOY_CENTER_MAX) {
    return 127;  // 중립
  }
  
  if (inverted < JOY_CENTER_MIN) {
    // 중립 아래 (0~126)
    return map(inverted, JOY_MIN, JOY_CENTER_MIN, 0, 126);
  } else {
    // 중립 위 (128~255)
    return map(inverted, JOY_CENTER_MAX, JOY_MAX, 128, 255);
  }
}

byte mapAileron(int rawValue) {
  // Aileron: 왼쪽(1023), 오른쪽(0)
  // rawValue: 0(오른쪽) ~ 1023(왼쪽)
  // output: 0(오른쪽) ~ 255(왼쪽), 127=중립
  
  // 데드존 적용
  if (rawValue >= JOY_CENTER_MIN && rawValue <= JOY_CENTER_MAX) {
    return 127;  // 중립
  }
  
  if (rawValue < JOY_CENTER_MIN) {
    // 중립 아래 (0~126) - 오른쪽
    return map(rawValue, JOY_MIN, JOY_CENTER_MIN, 0, 126);
  } else {
    // 중립 위 (128~255) - 왼쪽
    return map(rawValue, JOY_CENTER_MAX, JOY_MAX, 128, 255);
  }
}

// ========================================
// 조이스틱 값 읽기
// ========================================
void readJoysticks() {
  int raw_throttle = analogRead(JOYSTICK_THROTTLE_PIN);   // A1
  int raw_aileron = analogRead(JOYSTICK_AILERON_PIN);     // A0
  int raw_elevator = analogRead(JOYSTICK_ELEVATOR_PIN);   // A2
  int raw_rudder = analogRead(JOYSTICK_RUDDER_PIN);       // A3 (미사용)
  
  data.throttle = mapThrottle(raw_throttle);    // 값 반전 적용
  data.aileron = mapAileron(raw_aileron);       // 정방향
  data.elevator = mapElevator(raw_elevator);    // 값 반전 적용
  data.rudder = 127;  // 미사용 (중립)
}

// ========================================
// 초기화
// ========================================
void setup() {
  #if DEBUG_MODE
    Serial.begin(9600);
    Serial.println("=== RC Glider Transmitter ===");
    Serial.println();
  #endif
  
  // RF 모듈 초기화 (재시도 포함)
  #if DEBUG_MODE
    Serial.println("Initializing NRF24L01...");
  #endif
  
  bool radioOK = false;
  for (int i = 0; i < 3; i++) {
    if (initRadio()) {
      radioOK = true;
      break;
    }
    #if DEBUG_MODE
      Serial.print("Init attempt ");
      Serial.print(i + 1);
      Serial.println(" failed, retrying...");
    #endif
    delay(500);
  }
  
  if (!radioOK) {
    #if DEBUG_MODE
      Serial.println("ERROR: NRF24L01 init failed!");
      Serial.println("Check connections:");
      Serial.println("  VCC -> 3.3V");
      Serial.println("  GND -> GND");
      Serial.println("  CE  -> D2");
      Serial.println("  CSN -> D3");
      Serial.println("  SCK -> D13");
      Serial.println("  MOSI-> D11");
      Serial.println("  MISO-> D12");
    #endif
    while (1);  // 초기화 실패 시 멈춤
  }
  
  #if DEBUG_MODE
    Serial.println("NRF24L01 initialized OK!");
    Serial.println();
    Serial.println("Settings:");
    Serial.println("  Channel: 108");
    Serial.println("  Data Rate: 250kbps");
    Serial.println("  PA Level: MAX");
    Serial.println("  Auto-ACK: ON");
    Serial.println();
    Serial.println("Joystick Mapping:");
    Serial.println("  A1 (JOY1 Y): Throttle (Up=Max, Down=Min)");
    Serial.println("  A0 (JOY2 X): Aileron (Left=Max, Right=Min)");
    Serial.println("  A2 (JOY2 Y): Elevator (Up=Max, Down=Min)");
    Serial.println();
    Serial.print("  Center: ");
    Serial.print(JOY_CENTER_MIN);
    Serial.print(" ~ ");
    Serial.println(JOY_CENTER_MAX);
    Serial.println();
    Serial.println("Ready to transmit!");
    Serial.println("================================");
    Serial.println();
  #endif
  
  // 초기 데이터 설정
  data.throttle = 0;
  data.aileron = 127;
  data.elevator = 127;
  data.rudder = 127;
}

// ========================================
// 메인 루프
// ========================================
void loop() {
  unsigned long now = millis();
  
  // 20ms마다 전송 (50Hz)
  if (now - lastTransmitTime >= TRANSMIT_INTERVAL) {
    lastTransmitTime = now;
    
    // 조이스틱 값 읽기
    readJoysticks();
    
    // RF 전송 (재시도 포함)
    bool success = false;
    for (int i = 0; i < 2; i++) {
      success = radio.write(&data, sizeof(Signal));
      if (success) break;
      delayMicroseconds(250);
    }
    
    #if DEBUG_MODE
      transmitCount++;
      transmitSuccess = success;
      
      if (!success) {
        failCount++;
      } else {
        failCount = 0;
      }
      
      // 10회 연속 실패 시 경고
      if (failCount >= 10 && failCount % 10 == 0) {
        Serial.println("!!! WARNING: Connection unstable !!!");
      }
      
      // 50회 연속 실패 시 재초기화
      if (failCount >= 50) {
        Serial.println("!!! Reinitializing radio !!!");
        initRadio();
        failCount = 0;
      }
    #endif
  }
  
  // ========================================
  // 디버그 출력 (1초마다)
  // ========================================
  #if DEBUG_MODE
    if (now - lastPrintTime >= 1000) {
      char buffer[120];

      sprintf(buffer, "TX:%2d/s [%s]", transmitCount, transmitSuccess ? "OK" : "FAIL");
      Serial.print(buffer);
      
      if (failCount > 0) {
        sprintf(buffer, " Fails:%2d", failCount);
        Serial.print(buffer);
      }
      
      sprintf(buffer, "\t| T:%3d | A:%3d | E:%3d", data.throttle, data.aileron, data.elevator);
      Serial.print(buffer);
      
      // 조이스틱 raw 값 출력 (캘리브레이션용)
      sprintf(buffer, " |\t [Raw] T(A1)=%4d | A(A0)=%4d | E(A2)=%4d", 
              analogRead(JOYSTICK_THROTTLE_PIN),
              analogRead(JOYSTICK_AILERON_PIN),
              analogRead(JOYSTICK_ELEVATOR_PIN));
      Serial.println(buffer);

      
      lastPrintTime = now;
      transmitCount = 0;
    }
  #endif
}
