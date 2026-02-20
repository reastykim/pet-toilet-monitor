# XIAO ESP32-C6 보드 레퍼런스

Seeed Studio XIAO ESP32-C6 보드의 핀아웃, 핀 이름 체계, 주요 사양 정리.

---

## 핀아웃 다이어그램

```
XIAO ESP32-C6 상단면 (USB-C 포트가 위쪽)

                        USB-C
                         ┌──────┐
                  (좌측)  │      │ (우측)
    LP_GPIO0/GPIO0/A0/D0 ┤●    ●├ 5V
    LP_GPIO1/GPIO1/A1/D1 ┤●    ●├ GND
    LP_GPIO2/GPIO2/A2/D2 ┤●    ●├ 3V3
    SDIO_DATA1/GPIO21/D3 ┤●    ●├ D10/MOSI/GPIO18/SDIO_CMD
SDIO_DATA2/GPIO22/SDA/D4 ┤●    ●├ D9/MISO/GPIO20/SDIO_DATA0
SDIO_DATA3/GPIO23/SCL/D5 ┤●    ●├ D8/SCK/GPIO19/SDIO_CLK
            GPIO16/TX/D6 ┤●    ●├ D7/RX/GPIO17
                         └──────┘
```

> ⚠️ **주의**: GND와 3V3은 **우측** 핀입니다. 좌측에는 GND/3V3이 없습니다.

---

## 핀 이름 체계 설명

하나의 물리적 핀에 여러 이름이 붙는 이유는 **핀 멀티플렉싱(Pin Multiplexing)** 때문입니다.
같은 핀을 어떤 기능으로 사용하느냐에 따라 부르는 이름이 달라집니다.

| 표기 예시 | 이름 체계 | 의미 | 사용 예 |
|-----------|----------|------|--------|
| `A0`, `A1`, `A2` | **아날로그 핀 번호** (보드 라벨) | XIAO PCB에 인쇄된 아날로그 입력용 번호. ADC 가능 핀에만 붙음 | 배선 연결 시 |
| `D0`~`D10` | **디지털 핀 번호** (보드 라벨) | XIAO PCB에 인쇄된 디지털 I/O 번호. Arduino 스타일 | 배선 연결 시 |
| `GPIO0`~`GPIO23` | **GPIO 번호** (칩 레벨) | ESP32-C6 칩이 내부적으로 부르는 번호. ESP-IDF 코드에서 사용 | `gpio_set_level(GPIO_NUM_0, 1)` |
| `LP_GPIO0`~`LP_GPIO2` | **저전력 GPIO** (LP = Low Power) | 딥슬립 중에도 동작 가능한 ULP(Ultra-Low-Power) 전용 핀 | 저전력 웨이크업 감지 |
| `ADC1_CH0`~`ADC1_CH2` | **ADC 채널 번호** | 아날로그→디지털 변환기 내부 채널 번호. ADC API에서 사용 | `ADC_CHANNEL_0` |
| `SDA`, `SCL` | **I2C 버스** | I2C 통신의 데이터(SDA)·클럭(SCL) 신호선 | DHT22, OLED 등 |
| `TX`, `RX` | **UART 시리얼** | 직렬 통신 송신(TX)·수신(RX) | 시리얼 콘솔, GPS 등 |
| `SCK`, `MOSI`, `MISO` | **SPI 버스** | SPI 통신의 클럭·데이터 출력·데이터 입력 | SD카드, 디스플레이 등 |
| `SDIO_*` | **SDIO 버스** | SD카드 / Wi-Fi·BT 내부 버스 (일반 용도로는 사용 안 함) | 내부 전용 |

---

## 전체 핀 목록

| 보드 라벨 | GPIO | ADC 채널 | LP GPIO | 주요 기능 | 비고 |
|-----------|------|---------|---------|----------|------|
| A0 / D0 | GPIO0 | ADC1_CH0 | LP_GPIO0 | ADC, GPIO, SPI | ← MQ-135 AO 연결 (현재) |
| A1 / D1 | GPIO1 | ADC1_CH1 | LP_GPIO1 | ADC, GPIO |  |
| A2 / D2 | GPIO2 | ADC1_CH2 | LP_GPIO2 | ADC, GPIO |  |
| D3 | GPIO21 | — | — | GPIO, SDIO_DATA1 |  |
| D4 / SDA | GPIO22 | — | — | I2C SDA, SDIO_DATA2 |  |
| D5 / SCL | GPIO23 | — | — | I2C SCL, SDIO_DATA3 |  |
| D6 / TX | GPIO16 | — | — | UART0 TX |  |
| D7 / RX | GPIO17 | — | — | UART0 RX |  |
| D8 / SCK | GPIO19 | — | — | SPI SCK, SDIO_CLK |  |
| D9 / MISO | GPIO20 | — | — | SPI MISO, SDIO_DATA0 |  |
| D10 / MOSI | GPIO18 | — | — | SPI MOSI, SDIO_CMD |  |
| — | GPIO15 | — | — | 온보드 LED | Active-Low |
| 5V | — | — | — | 전원 출력 (USB로부터) | 최대 500mA |
| GND | — | — | — | 접지 |  |
| 3V3 | — | — | — | 3.3V 전원 출력 | 최대 700mA |

> **ADC 가능 핀**: A0(GPIO0), A1(GPIO1), A2(GPIO2) — 3핀만 ADC 지원
> **LP(저전력) 핀**: A0~A2만 딥슬립 중 동작 가능

---

## 주요 사양

| 항목 | 값 |
|------|----|
| MCU | ESP32-C6 (RISC-V 32-bit, 160 MHz) |
| Flash | 4 MB |
| RAM | 512 KB SRAM |
| 무선 | Wi-Fi 6 (802.11ax), Bluetooth 5, **Zigbee/Thread (802.15.4)** |
| GPIO | 11핀 (사용 가능) |
| ADC | ADC1: 3채널 (A0~A2), 12비트 |
| I2C | 1개 (SDA=D4/GPIO22, SCL=D5/GPIO23) |
| SPI | 1개 (SCK=D8, MOSI=D10, MISO=D9) |
| UART | 1개 (TX=D6/GPIO16, RX=D7/GPIO17) |
| 동작 전압 | 3.3V (GPIO 신호 레벨) |
| 공급 전압 | 5V (USB-C) 또는 3.7V (배터리) |
| ADC 최대 입력 | ~3.1V (`ADC_ATTEN_DB_12` 기준) |
| 온보드 LED | GPIO15 (Active-Low) |
| 크기 | 21 × 17.5 mm |

---

## ADC 사용 시 주의사항

```
ESP32-C6 ADC 입력 전압 제한: 최대 약 3.1V (ADC_ATTEN_DB_12 설정 시)

5V 신호를 직접 연결하면 보드가 손상됩니다.
→ 5V 센서 출력은 반드시 전압 분배기를 통해 3.3V 이하로 낮춰야 합니다.
```

| 감쇠 설정 | 입력 범위 | 용도 |
|-----------|---------|------|
| `ADC_ATTEN_DB_0` | 0 ~ 750 mV | 저전압 신호 |
| `ADC_ATTEN_DB_2_5` | 0 ~ 1.05 V | — |
| `ADC_ATTEN_DB_6` | 0 ~ 1.3 V | — |
| `ADC_ATTEN_DB_12` | 0 ~ **3.1 V** | 3.3V 센서 (기본 권장) |

---

## 이 프로젝트에서의 사용

| 핀 | 연결 대상 | 역할 |
|----|----------|------|
| A0 (GPIO0, ADC1_CH0) | MQ-135 AO | NH₃ 아날로그 신호 읽기 |
| GPIO15 | 온보드 LED | Zigbee 상태 표시 (Active-Low) |
| 내장 802.15.4 | SmartThings Hub | Zigbee 3.0 통신 |

---

## 참고 문서

- [Seeed Studio XIAO ESP32-C6 Getting Started](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
- [XIAO ESP32-C6 핀 멀티플렉싱](https://wiki.seeedstudio.com/xiao_pin_multiplexing_esp32c6/)
- [ESP32-C6 데이터시트](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [ESP-IDF GPIO API](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32c6/api-reference/peripherals/gpio.html)
- [ESP-IDF ADC Oneshot API](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32c6/api-reference/peripherals/adc_oneshot.html)

---

_Last Updated: 2026-02-21_
