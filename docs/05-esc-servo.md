# ESC와 서보 제어

수신된 Signal 구조체의 byte 값을 ESC PWM 펄스(µs)와 서보 각도(°)로 변환하여 모터와 조종면을 구동하는 방법을 설명합니다.

ESC와 서보는 모두 PWM 신호로 제어되지만 값의 범위와 의미가 다릅니다. ESC는 마이크로초 단위 펄스 폭으로 모터 속도를 제어하고, 서보는 각도로 조종면 위치를 설정합니다.

---

<br>

## ESC 제어

### 펄스 범위

| 펄스 | 동작 |
|------|------|
| 1500µs (`ESC_STOP`) | 완전 정지 |
| 1510µs (`ESC_MIN`) | 최소 작동 속도 |
| 2000µs (`ESC_MAX`) | 최대 속도 |

`ESC_MIN`을 1500µs가 아닌 1510µs로 설정하는 이유는 ESC가 정지 신호(1500µs)와 최저 속도를 명확히 구분하도록 하기 위해서다.

### 매핑 코드

```cpp
#define ESC_STOP  1500
#define ESC_MIN   1510
#define ESC_MAX   2000

int motor_pulse;
if (data.throttle == 0) {
    motor_pulse = ESC_STOP;                              // throttle=0 → 완전 정지
} else {
    motor_pulse = map(data.throttle, 1, 255, ESC_MIN, ESC_MAX);  // 1~255 → 1510~2000µs
}
motorESC.writeMicroseconds(motor_pulse);
```

`data.throttle == 0`과 `1~255`를 분기하여 아주 낮은 Throttle 값에서도 모터가 의도치 않게 작동하지 않도록 한다.

### 초기화

ESC는 `setup()`에서 `attach()` 직후 정지 신호를 보내고 2초 대기한다. ESC가 전원 인가 시 정지 신호를 확인하는 캘리브레이션 절차를 거치기 때문이다.

```cpp
motorESC.attach(MOTOR_ESC_PIN);
motorESC.writeMicroseconds(ESC_STOP);
delay(2000);  // ESC 초기화 대기
```

---

<br>

## 서보 제어

### 각도 범위

```cpp
#define SERVO_MIN  30   // 좌측/하강 최대
#define SERVO_MAX  150  // 우측/상승 최대
```

30~150° (120° 범위)로 제한하는 이유는 조종면의 기계적 한계를 초과하지 않도록 하기 위해서다.

### 매핑 코드

```cpp
int aileron_target  = map(data.aileron,  0, 255, SERVO_MIN, SERVO_MAX);
int elevator_target = map(data.elevator, 0, 255, SERVO_MIN, SERVO_MAX);
```

| `data.aileron` | 서보 각도 | 의미 |
|---------------|---------|------|
| 0 | 30° | 우측 최대 |
| 127 | 90° | 중립 |
| 255 | 150° | 좌측 최대 |

---

<br>

## 서보 초기화

```cpp
servoAileron.attach(SERVO_AILERON_PIN);
servoElevator.attach(SERVO_ELEVATOR_PIN);

// ESC 초기화와 동일한 2초 대기 후 중립 설정
servoAileron.write(90);
servoElevator.write(90);
```

---

<br>

## 참고 사항

- 서보 스무딩, 데드밴드, 자동 절전, 페일세이프 동작은 [06-signal-stability.md](./06-signal-stability.md)를 참고하세요.
- Signal 구조체의 byte 값이 어떻게 생성되는지는 [03-joystick-mapping.md](./03-joystick-mapping.md)를 참고하세요.
