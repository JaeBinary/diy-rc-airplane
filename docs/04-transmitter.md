# 송신기 전송 루프

20ms마다 조이스틱 값을 읽어 RF로 전송하고, 연속 실패 시 라디오를 자동으로 재초기화하는 방법을 설명합니다.

50Hz 전송 주기를 유지하면서 통신 실패를 자동으로 복구하는 것이 핵심입니다. `delay()` 대신 `millis()` 비교로 타이밍을 관리하므로 시리얼 출력과 간섭 없이 정확한 주기를 유지할 수 있습니다.

---

<br>

## 전송 루프 구조

```cpp
const unsigned long TRANSMIT_INTERVAL = 20;  // 50Hz
unsigned long lastTransmitTime = 0;

void loop() {
    unsigned long now = millis();

    if (now - lastTransmitTime >= TRANSMIT_INTERVAL) {
        lastTransmitTime = now;
        readJoysticks();          // 조이스틱 읽기 → Signal 구조체 갱신

        bool success = false;
        for (int i = 0; i < 2; i++) {            // 최대 2회 재시도
            success = radio.write(&data, sizeof(Signal));
            if (success) break;
            delayMicroseconds(250);
        }

        if (!success) failCount++;
        else          failCount = 0;

        // 50회 연속 실패 → 라디오 재초기화
        if (failCount >= 50) {
            initRadio();
            failCount = 0;
        }
    }
}
```

---

<br>

## 타이밍 설계

`delay(20)` 대신 `millis()` 차이를 비교하는 이유는 시리얼 출력(디버그 모드)과 같은 다른 작업이 전송 타이밍에 영향을 주지 않도록 하기 위해서다.

| 항목 | 값 | 설명 |
|------|-----|------|
| 전송 주기 | 20ms (50Hz) | 실시간 조종에 충분한 갱신 속도 |
| 재시도 횟수 | 최대 2회 | 일시적 충돌 대응 |
| 재시도 지연 | 250µs | 채널 비워주기 |
| 재초기화 임계값 | 50회 연속 실패 | 장기 통신 불량 복구 |

---

<br>

## 초기화 재시도

`setup()`에서 `initRadio()`가 3회 연속 실패하면 펌웨어가 `while(1)`로 멈추고 에러 메시지를 출력한다.

```cpp
bool radioOK = false;
for (int i = 0; i < 3; i++) {
    if (initRadio()) { radioOK = true; break; }
    delay(500);
}
if (!radioOK) { while (1); }  // 초기화 실패 → 정지
```

---

<br>

## 디버그 모드 출력

`#define DEBUG_MODE 1`로 설정하면 1초마다 전송 통계를 출력한다.

```
TX:50/s [OK]    | T:128 | A:127 | E:127 |    [Raw] T(A1)=512 | A(A0)=510 | E(A2)=515
TX:48/s [FAIL] Fails: 2  | T:0 | A:127 | E:127 |    [Raw] T(A1)=512 | A(A0)=510 | E(A2)=515
```

| 항목 | 의미 |
|------|------|
| `TX:50/s` | 초당 전송 횟수 (목표: 50) |
| `[OK]` / `[FAIL]` | 마지막 전송 성공 여부 |
| `Fails:N` | 연속 실패 횟수 |
| `T / A / E` | 매핑된 Throttle / Aileron / Elevator 출력값 |
| `[Raw]` | 조이스틱 ADC 원시값 |

---

<br>

## 참고 사항

- RF 파라미터 설정과 Signal 구조체는 [02-rf-communication.md](./02-rf-communication.md)를 참고하세요.
- 조이스틱 ADC 값의 매핑 방법은 [03-joystick-mapping.md](./03-joystick-mapping.md)를 참고하세요.
