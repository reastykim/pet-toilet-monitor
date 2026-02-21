# MQ-135 캘리브레이션 절차 및 테스트 계획

> **대상 파일**: `main/air_sensor_driver_MQ135.c`
> **수정 상수**: `MQ135_R0_KOHM` (현재 임시값: 10.0 kΩ)
> **현재 상태**: 5V + 100kΩ:100kΩ 분배기 구성으로 전환. R0 재캘리브레이션 필요.
> **전환 이유**: 3.3V 구동 시 소변 모래 덩어리 테스트에서 2 ppm 이하로 이벤트 감지 불가 확인 (2026-02-22)

---

## 1. 배경 — R0란 무엇인가

MQ-135는 가스 농도에 따라 내부 저항(Rs)이 변하는 센서다.
**R0**는 신선한 공기(깨끗한 실외)에서 측정한 기준 저항값이며, 모든 ppm 계산의 기준이 된다.

```
NH₃ ppm = 102.2 × (Rs / R0)^(-2.473)
```

R0가 틀리면 ppm 값 전체가 비례적으로 왜곡된다.
예를 들어 실제 R0가 30 kΩ인데 44.7 kΩ으로 설정하면 NH₃ 농도가 실제보다 ~2배 과대 추정된다.

현재 드라이버의 R0 계산식 (5V + 분배기 기준):

```
V_adc        = raw / 4095 × 3.3          ← ADC가 읽은 전압 (분배기 출력)
AOUT_sensor  = V_adc × 2.0               ← 분배기 이전 실제 AOUT
Rs_clean_air = RL × (VCC − AOUT) / AOUT  ← VCC = 5.0V
R0           = Rs_clean_air / 3.6         ← 3.6: MQ-135 데이터시트 clean-air 비율
```

---

## 2. 준비물

| 항목 | 사양 | 비고 |
|------|------|------|
| 디바이스 | XIAO ESP32-C6 + MQ-135 **5V + 분배기 배선 완료** | 5V→VCC, AOUT→100kΩ→GPIO0, 분기→100kΩ→GND |
| 환경 | 신선한 실외 공기 | 주방·화장실·주차장 제외 |
| 시간 | 최소 30분 | 20분 웜업 + 10분 안정화 측정 |
| 도구 | 시리얼 모니터 (ESP-IDF `idf.py monitor` 또는 PuTTY) | raw_adc, Rs 값 읽기 |
| 선택 | 멀티미터 | AOUT 전압 직접 확인 |

---

## 3. R0 측정 절차 (Step-by-Step)

### Step 1. 로그 레벨을 DEBUG로 변경

`sdkconfig` 또는 `build.ps1` 실행 전에 시리얼 로그 레벨 확인.
`air_sensor_driver_MQ135.c`의 `ESP_LOGD`가 출력되려면 `MQ135` 태그의 로그 레벨이 DEBUG여야 한다.

```
idf.py menuconfig
  → Component config → Log output → Default log verbosity → Debug
```

또는 런타임에서:
```c
esp_log_level_set("MQ135", ESP_LOG_DEBUG);
```

### Step 2. 신선한 실외에서 부팅

- 디바이스를 실외(베란다, 창문 밖 등)로 이동
- USB 연결 후 `idf.py monitor` 실행
- 부팅 후 웜업 로그 확인:
  ```
  I MQ135: MQ-135 initialized on GPIO0 (ADC1_CH0), R0=44.7 kΩ, warmup 20000 ms
  ```

### Step 3. 20분 대기 (웜업)

- 웜업 중에는 `[WARMUP]` 태그가 붙은 로그 출력
- **웜업 완료 전 데이터는 무시**

```
D MQ135: raw=240 V=0.193 Rs=159.2kΩ Rs/R0=3.56 NH3=52.1ppm [WARMUP]
```

### Step 4. 안정화 데이터 수집 (10분)

웜업 완료 후 `[WARMUP]` 태그가 사라지면 10분간 raw_adc 값을 기록한다.

```
D MQ135: raw=312 V=0.251 Rs=121.8kΩ Rs/R0=2.72 NH3=18.3ppm
D MQ135: raw=318 V=0.256 Rs=119.5kΩ Rs/R0=2.67 NH3=17.8ppm
D MQ135: raw=310 V=0.250 Rs=122.2kΩ Rs/R0=2.73 NH3=18.5ppm
```

60초마다 10회 이상 수집하여 **평균 raw_adc** 계산.

### Step 5. R0 계산

평균 raw_adc로 R0를 계산한다:

```
VCC  = 3.3 V
RL   = 10 kΩ
raw  = 측정 평균값 (예: 315)

V    = raw / 4095 × 3.3  =  315 / 4095 × 3.3  =  0.254 V
Rs   = RL × (VCC - V) / V  =  10 × (3.3 - 0.254) / 0.254  =  119.9 kΩ
R0   = Rs / 3.6  =  119.9 / 3.6  =  33.3 kΩ
```

**계산기 (Python 스크립트):**

```python
raw_avg = 315          # 측정 평균값 입력
VCC = 3.3
RL  = 10.0
V   = raw_avg / 4095.0 * VCC
Rs  = RL * (VCC - V) / V
R0  = Rs / 3.6
print(f"V = {V:.3f} V")
print(f"Rs = {Rs:.2f} kΩ")
print(f"R0 = {R0:.2f} kΩ  ← 이 값을 MQ135_R0_KOHM에 입력")
```

### Step 6. 코드 반영 및 재빌드

`main/air_sensor_driver_MQ135.c` 수정:

```c
/* 기존 */
#define MQ135_R0_KOHM   44.7f

/* 측정값으로 교체 (예: 33.3) */
#define MQ135_R0_KOHM   33.3f
```

`build.ps1` 실행 후 플래시:

```powershell
powershell.exe -ExecutionPolicy Bypass -File build.ps1
```

### Step 7. 재측정으로 검증

신선한 공기에서 재부팅 후 NH₃ ppm이 **0~5 ppm** 범위에 수렴하는지 확인.
실내 공기 기준으로 5~20 ppm이면 정상 범위.

---

## 4. 이벤트 감지 테스트 계획

### 4-1. 인위적 NH₃ 자극 테스트 (실내)

캘리브레이션 완료 후, 이벤트 감지 로직(event_detector)이 올바르게 동작하는지 확인한다.

**준비물:**
- 암모니아 발생원: 고양이 모래 소량(10g) + 따뜻한 물 10mL → 밀폐 컵에 혼합
- 또는: 유리 세정제(암모니아 함유) 소량

**테스트 절차:**

| 단계 | 행동 | 기대 결과 |
|------|------|-----------|
| T+0s | 디바이스 정상 동작 확인 (IDLE 상태) | `D DETECTOR: state=IDLE baseline=X.Xppm` |
| T+30s | NH₃ 발생원을 센서 5cm 앞에 접근 | ppm 급상승 → `ACTIVE` 전환 |
| T+40s | 발생원 제거 | ppm 감소 시작 |
| T+2min | 자연 감쇠 대기 | `COOLDOWN` 전환 + 이벤트 분류 로그 |
| T+3min | SmartThings 앱 확인 | "소변 감지됨" 또는 "대변 감지됨" 표시 |

**예상 시리얼 로그:**

```
D DETECTOR: state=IDLE    baseline=8.2ppm  ppm=9.1
D DETECTOR: state=ACTIVE  baseline=8.2ppm  ppm=31.7  ticks=1
D DETECTOR: state=ACTIVE  baseline=8.2ppm  ppm=48.3  peak=48.3ppm  ticks=2
D DETECTOR: state=ACTIVE  baseline=8.2ppm  ppm=22.1  ticks=3
D DETECTOR: state=ACTIVE  baseline=8.2ppm  ppm=11.4  below=1  ticks=4
D DETECTOR: state=ACTIVE  baseline=8.2ppm  ppm=10.2  below=2  ticks=5
D DETECTOR: state=ACTIVE  baseline=8.2ppm  ppm=9.3   below=3  ticks=6
I DETECTOR: EVENT=URINATION  peak=48.3ppm  peak_ticks=2  delta=40.1ppm
D DETECTOR: state=COOLDOWN  cooldown_ticks=1/6
```

### 4-2. 배변 패턴 시뮬레이션 테스트

발생원을 서서히 접근시켜 완만한 상승 패턴을 만든다.

| 단계 | 행동 |
|------|------|
| T+0~60s | NH₃ 발생원을 30cm → 20cm → 10cm 서서히 접근 (1분에 걸쳐) |
| T+60s | 발생원 제거 |
| T+3min | 감지 결과 확인 |

**판정 기준:**
- peak_ticks > 3 이면 → `DEFECATION` (대변 감지됨)
- peak_ticks ≤ 3 이면 → `URINATION` (소변 감지됨)

### 4-3. 실제 화장실 테스트

| 항목 | 내용 |
|------|------|
| 설치 위치 | 화장실 내부, 모래 표면에서 10~20cm 위 |
| 테스트 기간 | 최소 3일 (고양이 자연 사용) |
| 확인 항목 | SmartThings 앱 알림 수신 여부, 오감지(false positive) 횟수 |
| 성공 기준 | 화장실 사용 시 3분 내 알림 수신, 24시간 오감지 < 1회 |

---

## 5. 이벤트 감지 파라미터 튜닝

실환경 테스트 결과에 따라 `event_detector.h`의 상수를 조정한다.

| 상수 | 현재값 | 의미 | 조정 가이드 |
|------|--------|------|-------------|
| `EVENT_TRIGGER_DELTA_PPM` | 10.0 | 이벤트 시작 임계값 (ppm) | 오감지 많으면 ↑, 미감지 많으면 ↓ |
| `EVENT_HYSTERESIS_PPM` | 3.0 | 이벤트 종료 여유값 (ppm) | 이벤트가 너무 빨리 끝나면 ↑ |
| `EVENT_END_TICKS` | 3 | 연속 기저선 근접 횟수 (30s) | 이벤트가 너무 길면 ↓ |
| `EVENT_COOLDOWN_TICKS` | 6 | 쿨다운 시간 (60s) | 연속 사용 감지 필요 시 ↓ |
| `URINE_FAST_PEAK_TICKS` | 3 | 배뇨 판정 기준 peak 속도 | 분류 오류 시 조정 |
| `BASELINE_ALPHA` | 0.05 | EMA 기저선 업데이트 계수 | 환경 변화 빠르면 ↑ |

---

## 6. 측정 결과 기록표

캘리브레이션 완료 후 아래 표를 채워서 `CLAUDE.md`에 기록한다.

| 항목 | 값 |
|------|-----|
| 측정 일시 | |
| 측정 환경 | |
| 평균 raw_adc | |
| 측정 전압 (V) | |
| Rs (kΩ) | |
| **R0 (kΩ)** | |
| 신선한 공기 NH₃ (ppm) | |
| 펌웨어 반영 날짜 | |

---

_작성일: 2026-02-21_
