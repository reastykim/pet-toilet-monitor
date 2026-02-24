# MQ-135 캘리브레이션 절차 및 테스트 계획

> **대상 파일**: `main/air_sensor_driver_MQ135.c`
> **수정 상수**: `MQ135_R0_KOHM`
> **현재 상태**: 캘리브레이션 완료 — R0 = **6.3 kΩ** (2026-02-23)
> **배선**: 5V(VBUS) + 100kΩ:100kΩ 전압 분배기 → GPIO0 (ADC1_CH0)

---

## 1. 배경 — R0란 무엇인가

MQ-135는 가스 농도에 따라 내부 저항(Rs)이 변하는 센서다.
**R0**는 신선한 공기(깨끗한 실외)에서 측정한 기준 저항값이며, 모든 ppm 계산의 기준이 된다.

```
NH₃ ppm = 102.2 × (Rs / R0)^(-2.473)
```

R0가 틀리면 ppm 값 전체가 비례적으로 왜곡된다.

현재 드라이버의 R0 계산식 (5V + 100kΩ:100kΩ 분배기 기준):

```
V_adc        = raw / 4095 × 3.3           ← ADC가 읽은 전압 (분배기 출력)
AOUT_sensor  = V_adc × 2.0                ← 분배기 이전 실제 AOUT
Rs_clean_air = RL × (VCC − AOUT) / AOUT  ← VCC = 5.0V, RL = 10kΩ
R0           = Rs_clean_air / 3.6         ← 3.6: MQ-135 데이터시트 clean-air 비율
```

---

## 2. R0의 본질 — 센서 개체차 vs 환경

### 핵심 결론

**R0는 환경이 아닌 개별 센서의 물리적 특성값이다.**

### 변동 원인

| 변동 원인 | 영향 크기 | 설명 |
|-----------|----------|------|
| **센서 개체차** | **매우 큼** | 반도체 산화물 층 두께/밀도 편차. 같은 로트에서도 ±20~30% 차이. MQ-135 데이터시트 Rs 범위: 10kΩ~200kΩ |
| **공급 전압** | 큼 | 히터 온도 변화 → 감지층 온도 → R0 변화. 3.3V에서 5V로 전환 시 재캘리브레이션 필수 |
| **노화** | 중간 (느림) | 수년에 걸쳐 서서히 드리프트 |
| 온도/습도 | 작음 | Rs에 일시적으로 영향. R0 자체는 거의 영향 없음 |

→ **같은 제품을 100개 만들면 R0가 100개 다 다르다.**

### 이 제품에서 R0 오차의 실제 영향

이벤트 감지 로직(`event_detector.c`)은 절대 ppm 값이 아닌 **상대 변화량(delta)**으로 동작한다:

```c
// 이벤트 트리거 조건
if (ppm > baseline + EVENT_TRIGGER_DELTA_PPM)   // 절대값 X, 상대 변화량 O
```

EMA(지수이동평균) baseline이 각 센서의 "현재 평상시" 수준에 자동으로 적응하므로,
R0가 다소 틀려도 **이벤트 감지 자체는 정상 동작**한다.

| R0 오차의 영향 | 결과 |
|---------------|------|
| 앱에 표시되는 NH₃ ppm 절대값 | 오차 발생 (R0 오류에 비례) |
| 이벤트 트리거 (baseline+10ppm) | **정상 동작** — baseline이 자동 적응 |

### 상용 제품 관점: 고객 캘리브레이션이 필요한가?

| 접근법 | 장점 | 단점 | 적합성 |
|--------|------|------|--------|
| **R0 고정값 사용** | 생산 단순, 비용 없음 | ppm 절대값 ±20~30% 오차 | **이 제품에 충분** — 이벤트 감지 목적 |
| **EOL 캘리브레이션** | 정확한 ppm 표시 | 클린룸 챔버, 자동화 지그, 생산 공정 추가 | 정량 ppm 보장이 필요한 산업용·의료용 |
| **사용자 캘리브레이션** | 배포 환경 반영 | UX 나쁨, 사용자 오류 가능성 높음 | 비권장 |

**결론**: 이 제품은 이벤트 감지가 핵심 기능이고 EMA baseline이 센서 개체차를 자동 보상하므로,
**고객 캘리브레이션 불필요**, EOL 캘리브레이션도 선택사항이다.
앱에서 ppm을 표시할 경우 "참고값"으로 안내하는 것으로 충분하다.

---

## 3. 준비물

| 항목 | 사양 | 비고 |
|------|------|------|
| 디바이스 | XIAO ESP32-C6 + MQ-135 **5V + 분배기 배선 완료** | 5V→VCC, AOUT→100kΩ→GPIO0, 분기→100kΩ→GND |
| 환경 | 신선한 실외 공기 또는 창문 열린 실내 | 주방·화장실·주차장 제외 |
| 시간 | 최소 30분 | 20분 웜업 + 10분 안정화 측정 |
| 도구 | 시리얼 모니터 (`idf.py monitor` 또는 PuTTY) | raw_adc 값 읽기 |
| 선택 | 멀티미터 | AOUT 전압 직접 확인 |

---

## 4. R0 측정 절차 (Step-by-Step)

### Step 1. 신선한 공기에서 부팅

- 창문을 열거나 실외(베란다 등)로 이동
- USB 연결 후 `idf.py monitor` 실행
- 부팅 후 초기화 로그 확인:
  ```
  I MQ135: MQ-135 initialized on GPIO0 (ADC1_CH0), VCC=5.0V divider=2.0 R0=6.3 kΩ, warmup 20000 ms
  ```

### Step 2. 30분 대기 (웜업 + 안정화)

- 웜업 중에는 `[WARMUP]` 태그가 붙은 로그 출력
- **웜업 완료 전 데이터는 무시**

```
I LITTERBOX: Sensor warming up (raw=961), NH3=4.6 ppm (unreliable) [WARMUP]
```

### Step 3. 안정화 데이터 수집

웜업 완료 후 10분간 (최소 6회 이상) raw 값을 기록한다.

```
I LITTERBOX: Reported NH3=4 ppm (4.6 ppm_f, baseline=4.6, raw=961)
I LITTERBOX: Reported NH3=4 ppm (4.9 ppm_f, baseline=4.7, raw=986)
I LITTERBOX: Reported NH3=4 ppm (4.3 ppm_f, baseline=4.6, raw=949)
```

스파이크(평균 ±10% 초과)는 제외하고 평균 raw_adc를 계산한다.

### Step 4. R0 계산

**계산식 (5V + 100kΩ:100kΩ 분배기 기준):**

```
raw  = 953  (측정 평균값 예시)

V_adc = raw / 4095 × 3.3  =  953 / 4095 × 3.3  =  0.768 V
AOUT  = V_adc × 2.0        =  0.768 × 2.0        =  1.536 V
Rs    = 10 × (5.0 - 1.536) / 1.536              =  22.55 kΩ
R0    = Rs / 3.6            =  22.55 / 3.6        =  6.3 kΩ
```

**계산기 (Python 스크립트):**

```python
raw_avg       = 953     # 측정 평균값 입력
VCC           = 5.0     # 센서 공급 전압 (V)
RL            = 10.0    # 모듈 내장 부하 저항 (kΩ)
DIVIDER_RATIO = 2.0     # 100kΩ:100kΩ 분배기

V_adc = raw_avg / 4095.0 * 3.3
AOUT  = V_adc * DIVIDER_RATIO
Rs    = RL * (VCC - AOUT) / AOUT
R0    = Rs / 3.6
print(f"V_adc = {V_adc:.3f} V")
print(f"AOUT  = {AOUT:.3f} V")
print(f"Rs    = {Rs:.2f} kΩ")
print(f"R0    = {R0:.2f} kΩ  ← 이 값을 MQ135_R0_KOHM에 입력")
```

### Step 5. 코드 반영 및 재빌드

`main/air_sensor_driver_MQ135.c` 수정:

```c
#define MQ135_R0_KOHM   6.3f   /* 측정값으로 교체 */
```

`build.ps1` 실행 후 플래시:

```powershell
powershell.exe -ExecutionPolicy Bypass -File build.ps1
powershell.exe -ExecutionPolicy Bypass -File flash.ps1
```

### Step 6. 재측정으로 검증

신선한 공기에서 재부팅 후 NH₃ ppm이 **0~8 ppm** 범위에 수렴하는지 확인.
(완전한 실외 깨끗한 공기: 1~4 ppm, 창문 열린 실내: 4~8 ppm 정상)

---

## 5. 측정 결과 기록표

| 항목 | 값 |
|------|-----|
| 측정 일시 | 2026-02-23 |
| 측정 환경 | 실내, 창문 열림, 예열 ~30분 |
| 평균 raw_adc | 953 (n=6, 스파이크 1개 제외) |
| V_adc | 0.768 V |
| AOUT | 1.536 V |
| Rs (clean air) | 22.55 kΩ |
| **R0** | **6.3 kΩ** |
| 캘리브레이션 후 NH₃ (신선한 공기) | 4~5 ppm |
| 이벤트 트리거 기준 | baseline(≈4~5 ppm) + 10 ppm = **14~15 ppm** |
| 펌웨어 반영 날짜 | 2026-02-23 |

---

## 6. 이벤트 감지 테스트 계획

### 6-1. 인위적 NH₃ 자극 테스트 (실내)

캘리브레이션 완료 후, 이벤트 감지 로직(event_detector)이 올바르게 동작하는지 확인한다.

**준비물:**
- 암모니아 발생원: 고양이 모래 소량(10g) + 따뜻한 물 10mL → 밀폐 컵에 혼합
- 또는: 유리 세정제(암모니아 함유) 소량

**테스트 절차:**

| 단계 | 행동 | 기대 결과 |
|------|------|-----------|
| T+0s | 디바이스 정상 동작 확인 (IDLE 상태) | `DETECTOR: state=IDLE baseline=X.Xppm` |
| T+30s | NH₃ 발생원을 센서 5cm 앞에 접근 | ppm 급상승 → `ACTIVE` 전환 |
| T+40s | 발생원 제거 | ppm 감소 시작 |
| T+2min | 자연 감쇠 대기 | `COOLDOWN` 전환 + 이벤트 분류 로그 |
| T+3min | SmartThings 앱 확인 | "소변 감지됨" 또는 "대변 감지됨" 표시 |

**예상 시리얼 로그:**

```
DETECTOR: state=IDLE    baseline=4.6ppm  ppm=4.8
DETECTOR: state=ACTIVE  baseline=4.6ppm  ppm=22.4  ticks=1
DETECTOR: state=ACTIVE  baseline=4.6ppm  ppm=38.1  peak=38.1ppm  ticks=2
DETECTOR: state=ACTIVE  baseline=4.6ppm  ppm=15.3  ticks=3
DETECTOR: state=ACTIVE  baseline=4.6ppm  ppm=8.2   below=1  ticks=4
DETECTOR: state=ACTIVE  baseline=4.6ppm  ppm=6.9   below=2  ticks=5
DETECTOR: state=ACTIVE  baseline=4.6ppm  ppm=5.1   below=3  ticks=6
DETECTOR: EVENT=URINATION  peak=38.1ppm  peak_ticks=2  delta=33.5ppm
DETECTOR: state=COOLDOWN  cooldown_ticks=1/6
```

### 6-2. 배변 패턴 시뮬레이션 테스트

발생원을 서서히 접근시켜 완만한 상승 패턴을 만든다.

| 단계 | 행동 |
|------|------|
| T+0~60s | NH₃ 발생원을 30cm → 20cm → 10cm 서서히 접근 (1분에 걸쳐) |
| T+60s | 발생원 제거 |
| T+3min | 감지 결과 확인 |

**판정 기준:**
- `peak_ticks > 3` → `DEFECATION` (대변 감지됨)
- `peak_ticks ≤ 3` → `URINATION` (소변 감지됨)

### 6-3. 실제 화장실 테스트

| 항목 | 내용 |
|------|------|
| 설치 위치 | 화장실 내부, 모래 표면에서 10~20cm 위 |
| 테스트 기간 | 최소 3일 (고양이 자연 사용) |
| 확인 항목 | SmartThings 앱 알림 수신 여부, 오감지(false positive) 횟수 |
| 성공 기준 | 화장실 사용 시 3분 내 알림 수신, 24시간 오감지 < 1회 |

#### 1차 실제 소변 테스트 결과 (2026-02-24) — 미감지

| 항목 | 값 |
|------|-----|
| 설치 위치 | 밀폐형 화장실, 센서 모래 표면 **1cm** 앞 |
| baseline | ~4.6 ppm |
| 소변 후 최대 ppm | 6.3 ppm |
| 최대 delta | **+1.6 ppm** |
| 트리거 임계값 | 10.0 ppm |
| 결과 | **미감지** (Event START 없음) |
| 샘플링 간격 | 10초 (당시 펌웨어) |

**원인 분석:**
1. 응고 모래(클럼핑 타입)가 NH₃를 매우 빠르게 흡착 → 센서까지 확산되는 농도 자체가 낮음
2. 10초 샘플링 간격이 짧은 스파이크를 놓칠 가능성 높음 (소변 후 즉시 모래로 덮음)

**적용된 조치:**
- ADC 샘플링 주기: **10초 → 2초** (commit `b336be1`)
- Zigbee 보고 주기는 10초 유지 (5틱마다)
- 트리거 임계값은 2초 샘플링으로 재수집 후 재검토 예정

---

## 7. 이벤트 감지 파라미터 튜닝

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

_최초 작성: 2026-02-21 / 최종 업데이트: 2026-02-24 (실제 소변 테스트 결과 추가, 샘플링 2초 수정)_
