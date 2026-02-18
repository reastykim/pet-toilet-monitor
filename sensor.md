# Sensor Reference - LitterBox.v1

## MQ-135 Gas Sensor

### 개요

MQ-135는 Hanwei/Winsen에서 제조하는 반도체 금속산화물(SnO₂) 가스 센서.
내부 히터가 SnO₂ 감지층을 200~300°C로 가열하면, 환원성 가스(NH₃ 등) 농도에 따라
전기 저항이 **감소**하는 원리로 동작한다.

**감지 가능 가스**: NH₃, CO₂, NOx, 알코올, 벤젠, 톨루엔, 아세톤, 연기
**이 프로젝트 타겟**: NH₃ (암모니아) — 고양이 소변에서 발생

### 핀 구성 (모듈 보드 기준)

| 핀 | 이름 | 설명 |
|----|------|------|
| 1 | VCC | +5V 전원 |
| 2 | GND | 접지 |
| 3 | DO | 디지털 출력 (온보드 가변저항 임계값, LM393 비교기) |
| 4 | AO | 아날로그 출력 (0~5V, 가스 농도에 비례) |

### 전기적 사양

| 파라미터 | 값 | 비고 |
|---------|-----|------|
| 히터 전압 (VH) | 5.0V ±0.1V | AC 또는 DC |
| 회로 전압 (VC) | 5.0V ±0.1V | DC |
| 히터 저항 (RH) | 31Ω ±3Ω | 제조사별 상이 (33Ω ±5% 변종 있음) |
| 히터 소비전력 (PH) | ≤ 800mW | 약 150~170mA @ 5V |
| 부하 저항 (RL) | 10kΩ ~ 47kΩ | 데이터시트 권장: 20kΩ. **모듈 실측 필수** |
| 감지 저항 (Rs) | 30kΩ ~ 200kΩ | @ 100ppm NH₃, 표준 시험 조건 |

> **주의**: 히터는 200mA 이상 필요 → ESP32-C6 GPIO로 구동 불가, 반드시 5V 전원 레일에서 공급

### 동작 조건

| 파라미터 | 값 |
|---------|-----|
| 동작 온도 | -10°C ~ +45°C |
| 동작 습도 | < 95% RH (비응결) |
| 표준 시험 조건 | 20°C ±2°C, 65% ±5% RH, RL = 20kΩ |
| O₂ 농도 | 21% (대기 표준) |

### 예열 시간

| 조건 | 예열 시간 |
|------|----------|
| 최초 사용 (첫 전원 인가) | **24시간 이상** 연속 가열 |
| 이후 전원 재투입 | 3~5분 (안정적), 60~120초 (대략적) |
| 최소 동작 | 20초 |

> 리터박스 모니터는 **히터를 항상 켜둔 상태**로 운용하는 것이 정확도에 유리.

---

### 감도 곡선 (NH₃)

#### 멱함수(Power Law) 모델

Rs/Ro와 가스 농도(ppm)의 관계는 log-log 스케일에서 선형:

```
ppm = a × (Rs/Ro)^b
```

#### NH₃ 계수

| 출처 | a | b |
|------|---|---|
| MQSensorsLib (miguel5612) | 102.2 | -2.473 |
| AmanSCoder/MQ135 | 101.3708 | -2.5082 |

**채택값**:
```c
#define MQ135_NH3_COEFF_A    102.2f
#define MQ135_NH3_COEFF_B    -2.473f
```

#### 참고: 다른 가스 계수 (MQSensorsLib)

| 가스 | a | b |
|------|---|---|
| CO | 605.18 | -3.937 |
| 알코올 | 77.255 | -3.18 |
| CO₂ | 110.47 | -2.862 |
| 톨루엔 | 44.947 | -3.445 |
| **NH₃** | **102.2** | **-2.473** |
| 아세톤 | 34.668 | -3.369 |

#### NH₃ Rs/Ro → ppm 참고 테이블

| NH₃ ppm | Rs/Ro (근사) |
|---------|-------------|
| 10 | ~2.20 |
| 20 | ~1.60 |
| 50 | ~1.10 |
| 100 | ~1.00 (교정 기준점) |
| 200 | ~0.65 |
| 300 | ~0.52 |

#### Clean Air Ratio

```
Rs/Ro (깨끗한 공기) = 3.6
```

이 상수는 R0 교정에 사용됨.

### NH₃ 감지 범위

- **유효 범위**: 10 ~ 300 ppm
- **정확도 최적 범위**: 10 ~ 200 ppm
- 10 ppm 미만에서는 곡선이 평탄해져 분해능이 급격히 떨어짐

---

### Rs 계산 (ADC 값 → 저항)

센서와 부하 저항(RL)이 전압 분배기를 구성:

```
    VCC (5V)
      |
     [Rs]   ← 센서 저항 (가변)
      |
      +---→ Vout (AO 핀)
      |
     [RL]   ← 부하 저항 (고정, 예: 20kΩ)
      |
     GND
```

```
Vout = VCC × RL / (Rs + RL)
Rs = RL × (VCC - Vout) / Vout
Rs = RL × ((VCC / Vout) - 1)
```

### ESP32-C6 ADC 인터페이스 (XIAO ESP32-C6)

#### ADC 사양

| 항목 | 값 |
|------|-----|
| 해상도 | 12비트 (0~4095) |
| ADC 유닛 | ADC1 (ESP32-C6에는 ADC1만 사용 가능) |
| 감쇄 | `ADC_ATTEN_DB_12` → 최대 약 **2450mV** |
| API | ADC Oneshot (ESP-IDF v5.5.2) |

#### XIAO ESP32-C6 ADC 핀 매핑

| XIAO 핀 | GPIO | ADC 채널 |
|---------|------|---------|
| D0/A0 | GPIO0 | ADC1_CH0 |
| D1/A1 | GPIO1 | ADC1_CH1 |
| D2/A2 | GPIO2 | ADC1_CH2 |

**권장**: A0 (GPIO0 / ADC1_CHANNEL_0)

#### 전압 레벨 문제 — 중요

MQ-135 아날로그 출력: **0~5V**
ESP32-C6 ADC 최대: **~2450mV** (ADC_ATTEN_DB_12)

**반드시 전압 분배기 필요**:

```
MQ-135 AO ---[R1 = 270kΩ]---+---→ ESP32-C6 A0 (GPIO0)
                             |
                          [R2 = 220kΩ]
                             |
                            GND
```

```
분배 비율 = R2 / (R1 + R2) = 220 / 490 ≈ 0.449
5V × 0.449 = 2.245V  (ADC 범위 내 안전)
```

#### ADC → ppm 변환 코드 로직

```c
#define VCC            5.0f       // MQ-135 회로 전압
#define RL             20.0f      // 부하 저항 (kΩ) — 모듈 실측 필수!
#define ADC_MAX        4095       // 12비트 ADC
#define ADC_VREF_MV    2450.0f    // ADC_ATTEN_DB_12 최대 전압 (mV)
#define VDIV_RATIO     0.449f     // 전압 분배 비율

// ADC 읽기 → Rs 계산
int adc_raw = adc_oneshot_read(...);
float v_adc = (adc_raw / (float)ADC_MAX) * (ADC_VREF_MV / 1000.0f);  // V
float v_out = v_adc / VDIV_RATIO;    // 실제 센서 출력 전압
float rs = RL * (VCC - v_out) / v_out;  // 센서 저항 (kΩ)

// ppm 계산
float ratio = rs / R0;  // R0은 교정값
float ppm_nh3 = 102.2f * powf(ratio, -2.473f);
```

---

### R0 교정 (Calibration)

R0 = 표준 조건(100ppm NH₃)에서의 센서 저항. 깨끗한 공기에서 측정한 Rs와
Clean Air Ratio(3.6)를 이용하여 산출.

#### 교정 절차

1. **최초 예열**: 24시간 이상 연속 가열
2. **환경**: 깨끗한 외부 공기 또는 환기된 실내 (가스 소스 없음)
3. **Rs 측정**: 50회 이상 읽기 평균

```c
float rs_sum = 0;
for (int i = 0; i < 50; i++) {
    int adc_raw = read_adc();
    float v_adc = (adc_raw / 4095.0f) * 2.45f;
    float v_out = v_adc / VDIV_RATIO;
    float rs = RL * (VCC - v_out) / v_out;
    rs_sum += rs;
    vTaskDelay(pdMS_TO_TICKS(100));
}
float rs_air = rs_sum / 50.0f;
```

4. **R0 계산**: `R0 = rs_air / 3.6`
5. **NVS 저장**: 전원 재투입 시 재교정 불필요하도록 비휘발성 메모리에 저장

#### 교정 주의사항

- **RL 실측 필수**: 저가 모듈은 1kΩ RL을 사용하는 경우가 많음 (데이터시트 권장 20kΩ과 다름)
- 개별 센서마다 R0이 다름 — 센서 교체 시 반드시 재교정
- 주기적 재교정 권장 (수 주마다)

---

### 온습도 보정

센서는 20°C / 65% RH에서 교정됨. 실환경에서는 보정 필요:

```c
// AmanSCoder/MQ135 라이브러리 보정 계수
#define CORA  0.00035f
#define CORB  0.02718f
#define CORC  1.39538f
#define CORD  0.0018f

float correction = CORA * t * t + CORB * t + CORC - (humidity - 33.0f) * CORD;
float rs_corrected = rs / correction;
```

> 온습도 센서(DHT22 등)를 추가하면 보정 가능. 현재 프로젝트에서는 보정 미적용.

---

### 한계점

| 한계 | 설명 |
|------|------|
| 교차 민감도 | 여러 가스에 동시 반응 → NH₃만 분리 측정 불가 |
| 정밀도 | ±10~20% (최적 조건), 정량 분석 부적합 |
| 개체차 | Rs 범위 30kΩ~200kΩ (같은 농도에서) |
| 저농도 분해능 | 10ppm 미만에서 곡선 평탄 → 실질적 감지 어려움 |
| 예열 시간 | 최초 24시간+, 이후 3~5분 |
| 소비전력 | ~800mW (히터) → 배터리 단독 운용 부적합 |
| 수명 | 2~5년 (연속 운용 시) |
| NH₃ 전용 아님 | 전용이 필요하면 MQ-137 (1~500ppm NH₃) 고려 |

---

### 고양이 화장실 NH₃ 농도 컨텍스트

고양이 소변에 포함된 **요소(urea)**가 박테리아에 의해 분해되어 NH₃ 생성:

```
CO(NH₂)₂ + H₂O → 2NH₃ + CO₂
```

신선한 소변은 거의 무취 → 시간 경과에 따라 NH₃ 농도 상승.

#### 예상 NH₃ 농도

| 상황 | NH₃ (ppm) | 비고 |
|------|-----------|------|
| 깨끗한 모래 (기저값) | 0 ~ 2 | 대기 배경 수준 |
| 화장실 주변 공기 | 3 ~ 4 | |
| 배뇨 직후 (덮개 없음) | 5 ~ 9 | |
| 24시간 후 (1마리) | 10 ~ 15 | 요소 분해 진행 |
| 며칠 미청소 | 15 ~ 50 | 상당한 NH₃ 축적 |
| 다묘/환기 불량 | 50+ | 100+ ppm 도달 가능 |

#### 감지 전략

1. **절대값보다 상대 변화** 모니터링 — 기저값 대비 Rs/Ro 변화율로 이벤트 감지
2. **변화율(delta) 기반 임계값** 사용 — 고정 ppm 임계값보다 효과적
3. 10초 간격 모니터링은 점진적 NH₃ 상승 감지에 적합
4. 배뇨 직후 NH₃는 낮을 수 있음 (10ppm 미만) → MQ-135 분해능 한계 구간
5. 보조 센싱(무게/적외선) 병행 고려

---

### 구현 상수 요약

```c
// MQ-135 센서 설정
#define MQ135_HEATER_VOLTAGE       5.0f      // V
#define MQ135_CIRCUIT_VOLTAGE      5.0f      // V
#define MQ135_LOAD_RESISTANCE      20.0f     // kΩ (모듈 실측 필수!)
#define MQ135_CLEAN_AIR_RATIO      3.6f      // Rs/Ro (깨끗한 공기)

// NH₃ 멱함수 계수: ppm = a × (Rs/Ro)^b
#define MQ135_NH3_COEFF_A          102.2f
#define MQ135_NH3_COEFF_B          -2.473f

// ESP32-C6 ADC 설정 (XIAO)
#define MQ135_ADC_CHANNEL          ADC_CHANNEL_0    // GPIO0 (A0)
#define MQ135_ADC_ATTEN            ADC_ATTEN_DB_12  // ~0-2450mV
#define MQ135_ADC_BITWIDTH         ADC_BITWIDTH_DEFAULT  // 12비트
#define MQ135_ADC_MAX              4095
#define MQ135_ADC_VREF_MV          2450.0f

// 전압 분배기 (5V → ESP32 안전 범위)
#define MQ135_VDIV_R1              270000.0f  // Ω
#define MQ135_VDIV_R2              220000.0f  // Ω
#define MQ135_VDIV_RATIO           (MQ135_VDIV_R2 / (MQ135_VDIV_R1 + MQ135_VDIV_R2))  // ~0.449

// 온습도 보정 계수
#define MQ135_TEMP_CORA            0.00035f
#define MQ135_TEMP_CORB            0.02718f
#define MQ135_TEMP_CORC            1.39538f
#define MQ135_HUM_CORD             0.0018f
```

### 참고 문서

- [MQ-135 데이터시트 (Hanwei)](https://www.electronicoscaldas.com/datasheet/MQ-135_Hanwei.pdf)
- [MQ135 매뉴얼 v1.4 (Winsen)](https://www.winsen-sensor.com/d/files/PDF/Semiconductor%20Gas%20Sensor/MQ135%20(Ver1.4)%20-%20Manual.pdf)
- [MQSensorsLib (GitHub)](https://github.com/miguel5612/MQSensorsLib)
- [AmanSCoder/MQ135 (GitHub)](https://github.com/AmanSCoder/MQ135)
- [ESP32-C6 ADC Oneshot API](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32c6/api-reference/peripherals/adc_oneshot.html)
- [XIAO ESP32-C6 핀 멀티플렉싱](https://wiki.seeedstudio.com/xiao_pin_multiplexing_esp32c6/)
