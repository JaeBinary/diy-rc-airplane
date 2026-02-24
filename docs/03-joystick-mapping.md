# 조이스틱 캘리브레이션과 채널 매핑

조이스틱 ADC 값(0~1023)을 채널별 특성에 맞게 0~255 byte로 변환하는 방법을 설명합니다.

조이스틱 중립점은 개체마다 다릅니다. 중립 범위(480~540)를 데드존으로 지정하면 손을 놓았을 때 서보가 흔들리지 않습니다. 채널별로 값 반전 여부와 매핑 방식이 다르므로 각각 별도의 함수로 분리합니다.

---

<br>

## 데드존 설정

```cpp
#define JOY_CENTER_MIN  480   // 중립 하한
#define JOY_CENTER_MAX  540   // 중립 상한
#define JOY_MIN           0
#define JOY_MAX        1023
```

중립 범위 내의 ADC 값은 모두 중립으로 처리한다. 실제 조이스틱의 중립값을 디버그 모드로 측정하여 ±30 범위로 설정하는 것이 권장된다.

---

<br>

## 채널별 매핑

### Throttle (A1)

조이스틱을 위로 밀면 최대 출력, 중립 이하는 모두 0(정지)으로 처리한다.

```cpp
byte mapThrottle(int rawValue) {
    int inverted = JOY_MAX - rawValue;  // 위=최대가 되도록 반전
    if (inverted < JOY_CENTER_MAX) return 0;
    return map(inverted, JOY_CENTER_MAX, JOY_MAX, 0, 255);
}
```

- 조이스틱을 완전히 위로 밀면 `255` (ESC 2000µs)
- 중립 이하(손을 놓거나 아래로 밀면) `0` (ESC 1500µs 정지)
- 중간 위치는 0~255로 선형 매핑

### Aileron (A0)

좌우 대칭 매핑이며 중립에서 127을 반환한다.

```cpp
byte mapAileron(int rawValue) {
    if (rawValue >= JOY_CENTER_MIN && rawValue <= JOY_CENTER_MAX)
        return 127;                               // 데드존 → 중립
    if (rawValue < JOY_CENTER_MIN)
        return map(rawValue, JOY_MIN, JOY_CENTER_MIN, 0, 126);    // 좌측
    return map(rawValue, JOY_CENTER_MAX, JOY_MAX, 128, 255);      // 우측
}
```

- 조이스틱 우측 끝 → `0` (서보 30°)
- 중립(데드존) → `127` (서보 90°)
- 조이스틱 좌측 끝 → `255` (서보 150°)

### Elevator (A2)

Aileron과 동일한 로직이지만 값을 반전(위=최대)한다.

```cpp
byte mapElevator(int rawValue) {
    int inverted = JOY_MAX - rawValue;  // 위=최대가 되도록 반전
    if (inverted >= JOY_CENTER_MIN && inverted <= JOY_CENTER_MAX)
        return 127;
    if (inverted < JOY_CENTER_MIN)
        return map(inverted, JOY_MIN, JOY_CENTER_MIN, 0, 126);
    return map(inverted, JOY_CENTER_MAX, JOY_MAX, 128, 255);
}
```

---

<br>

## 채널 요약

| 채널 | 핀 | 중립 출력값 | 반전 여부 | 특이사항 |
|------|-----|-----------|---------|---------|
| Throttle | A1 | 0 | O (위=최대) | 중립 이하 전부 0 |
| Aileron | A0 | 127 | X | 좌우 대칭 |
| Elevator | A2 | 127 | O (위=최대) | 좌우 대칭 + 반전 |

---

<br>

## 캘리브레이션 방법

1. `transmitter.ino`에서 `#define DEBUG_MODE 1`로 설정한다.
2. 시리얼 모니터(9600 baud)에서 조이스틱을 중립에 놓고 raw 값을 확인한다.

```
TX:50/s [OK]  | T:0 | A:127 | E:127 |  [Raw] T(A1)=512 | A(A0)=510 | E(A2)=515
```

3. 측정된 중립값(예: 512)을 기준으로 데드존을 설정한다.

```cpp
#define JOY_CENTER_MIN  (측정값 - 30)
#define JOY_CENTER_MAX  (측정값 + 30)
```

---

<br>

## 참고 사항

- 매핑된 값이 Signal 구조체를 통해 수신기로 전달되는 과정은 [02-rf-communication.md](./02-rf-communication.md)를 참고하세요.
- 수신기에서 byte 값이 서보 각도와 ESC 펄스로 변환되는 방법은 [05-esc-servo.md](./05-esc-servo.md)를 참고하세요.
