# 하드웨어 구성 및 핀 매핑

Arduino Nano 두 대에 nRF24L01+ 모듈, 조이스틱, ESC, 서보를 연결하여 송신기와 수신기를 구성하는 방법을 설명합니다.

하드웨어 구성을 먼저 이해하면 이후 펌웨어의 핀 정의와 초기화 순서를 쉽게 파악할 수 있습니다.

---

<br>

## 송신기 구성

| 구성요소 | 모델 | 수량 | 핀 |
|---------|------|------|----|
| MCU | Arduino Nano (ATmega328P) | 1 | — |
| RF 모듈 | nRF24L01+ | 1 | CE=D2, CSN=D3, SPI |
| 조이스틱 1 (Throttle) | 2축 아날로그 | 1 | A1 (위아래 → 모터 속도) |
| 조이스틱 2 (Aileron/Elevator) | 2축 아날로그 | 1 | A0 (좌우 → Aileron), A2 (위아래 → Elevator) |

---

<br>

## 수신기 구성

| 구성요소 | 모델 | 수량 | 핀 |
|---------|------|------|----|
| MCU | Arduino Nano (ATmega328P) | 1 | — |
| RF 모듈 | nRF24L01+ | 1 | CE=D2, CSN=D3, SPI |
| ESC | — | 1 | D8 (PWM) |
| 서보 Aileron | SG90 또는 호환 | 1 | D9 |
| 서보 Elevator | SG90 또는 호환 | 1 | D10 |

---

<br>

## 핀 매핑

### 송신기

| 핀 | 신호 | 방향 | 설명 |
|----|------|------|------|
| D2 | nRF24 CE | 출력 | RF 모듈 칩 인에이블 |
| D3 | nRF24 CSN | 출력 | SPI 칩 셀렉트 (Active LOW) |
| D11 | SPI MOSI | 출력 | RF 모듈 데이터 출력 |
| D12 | SPI MISO | 입력 | RF 모듈 데이터 입력 |
| D13 | SPI SCK | 출력 | SPI 클럭 |
| A0 | 조이스틱 2 X축 | 입력 (ADC) | Aileron 좌우 (0~1023) |
| A1 | 조이스틱 1 Y축 | 입력 (ADC) | Throttle 모터 속도 (0~1023) |
| A2 | 조이스틱 2 Y축 | 입력 (ADC) | Elevator 상하 (0~1023) |

### 수신기

| 핀 | 신호 | 방향 | 설명 |
|----|------|------|------|
| D2 | nRF24 CE | 출력 | RF 모듈 칩 인에이블 |
| D3 | nRF24 CSN | 출력 | SPI 칩 셀렉트 (Active LOW) |
| D8 | ESC PWM | 출력 | 모터 속도 제어 (1510~2000µs) |
| D9 | Aileron 서보 | 출력 | 좌우 조종면 (30~150°) |
| D10 | Elevator 서보 | 출력 | 상하 조종면 (30~150°) |
| D11 | SPI MOSI | 출력 | RF 모듈 데이터 출력 |
| D12 | SPI MISO | 입력 | RF 모듈 데이터 입력 |
| D13 | SPI SCK | 출력 | SPI 클럭 |

---

<br>

## 전원 연결 주의사항

- nRF24L01+ 모듈은 반드시 **3.3V**로 전원을 공급한다. 5V 연결 시 모듈이 손상된다.
- VCC-GND 사이에 **10µF 이상 전해 캐패시터**를 추가하면 전원 노이즈로 인한 초기화 실패를 방지할 수 있다.
- ESC는 별도 배터리로 전원을 공급하고, Arduino Nano와 **GND만 공유**한다.

---

<br>

## 참고 사항

- nRF24L01+ 무선 통신 설정과 Signal 구조체는 [02-rf-communication.md](./02-rf-communication.md)를 참고하세요.
- 조이스틱 ADC 값의 매핑 방법은 [03-joystick-mapping.md](./03-joystick-mapping.md)를 참고하세요.
