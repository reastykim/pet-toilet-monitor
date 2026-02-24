# LitterBox.v1 — 스마트 반려동물 화장실 모니터

> **ESP32-C6 + MQ-135 + Zigbee 3.0 + SmartThings** 연동 IoT 프로젝트
>
> 노령묘의 배뇨/배변 감지를 목표로 시작한 개인 스터디 프로젝트.
> 암모니아 센서 단독 방식의 한계를 실측으로 확인하고 마무리함.

---

## 목차

- [프로젝트 배경](#프로젝트-배경)
- [시스템 구성](#시스템-구성)
- [하드웨어 & 배선](#하드웨어--배선)
- [소프트웨어 아키텍처](#소프트웨어-아키텍처)
- [빌드 / 플래시 / 모니터](#빌드--플래시--모니터)
- [학습 내용 요약](#학습-내용-요약)
  - [ESP-IDF & Zigbee](#esp-idf--zigbee)
  - [커스텀 Zigbee 클러스터](#커스텀-zigbee-클러스터)
  - [SmartThings Edge Driver](#smartthings-edge-driver)
  - [MQ-135 센서 & 캘리브레이션](#mq-135-센서--캘리브레이션)
  - [전압 분배기 설계](#전압-분배기-설계)
- [이벤트 감지 알고리즘](#이벤트-감지-알고리즘)
- [실제 테스트 결과 & 한계점](#실제-테스트-결과--한계점)
- [트러블슈팅 기록](#트러블슈팅-기록)
- [상세 문서](#상세-문서)

---

## 프로젝트 배경

반려묘가 나이가 들면서 화장실에 들어가지만 배뇨/배변을 실패하는 경우가 생겼다.
이를 원격으로 모니터링하여 건강 이상 징후를 조기에 파악하기 위한 IoT 디바이스를 직접 만들고자 시작했다.

**핵심 요구사항**:
- 배뇨 성공 여부 감지 (입장만 한 경우 vs. 실제 배뇨)
- SmartThings 앱으로 실시간 알림
- 저비용 하드웨어 (ESP32 + 가스 센서)

---

## 시스템 구성

```
┌─────────────────────────────────────────────────────────────┐
│                    LitterBox.v1 디바이스                     │
│                                                             │
│   MQ-135 센서  ─── 전압분배기(100kΩ×2) ─── XIAO ESP32-C6     │
│   (5V, NH₃)              ↓                                  │
│                      ADC 읽기 (2초)                          │
│                      EMA 기저선 추적                         │
│                      이벤트 감지 상태 머신                    │
│                           ↓                                 │
│                  Zigbee 3.0 (802.15.4)                      │
└─────────────────────────────┬───────────────────────────────┘
                              │ Zigbee
                              ▼
                    SmartThings Hub
                              │
                              ▼
                    SmartThings 앱
                   (NH₃ ppm 표시 + 이벤트 알림)
```

---

## 하드웨어 & 배선

### 부품 목록

| 부품 | 모델 | 비고 |
|------|------|------|
| MCU | Seeed XIAO ESP32-C6 | Zigbee 3.0 내장 |
| 가스 센서 | MQ-135 | NH₃, 벤젠, 알코올 등 |
| 저항 | 100kΩ × 2 | 전압 분배기 |
| 전원 | USB-C (5V) | VBUS를 MQ-135에 공급 |

### 배선도

```
XIAO ESP32-C6                         MQ-135 모듈
┌─────────────┐                      ┌──────────────┐
│          5V │━━━━━━━━━━━━━━━━━━━━━━│VCC           │  ← VBUS (5V)
│         GND │━━━━━━━━━━━━━━━━━━━━━━│GND           │
│          A0 │◀━━[100kΩ]━━┬━━━━━━━━│AO            │
│  (GPIO0)    │            │         └──────────────┘
└─────────────┘         [100kΩ]        DO: 미연결
                            │
                           GND
```

> **왜 5V?** 3.3V 구동 시 히터 온도 부족 → 감도 급락 확인 (2026-02-22).
> **왜 분배기?** AO 최대 5V, ESP32-C6 ADC 최대 3.1V → 보호 필수.
> **왜 100kΩ?** 3가지 제약(Loading effect / ADC 소스 임피던스 / 전류소비)을 동시 만족하는 최적값.

자세한 이유 → [MQ-135 센서 레퍼런스](docs/MQ-135.md#왜-100kΩ인가----저항값-선정-이유)

### XIAO ESP32-C6 핀 배치

```
                      USB-C
                       ┌──────┐
      GPIO0/A0/D0/ADC1 ┤●    ●├ 5V  ← MQ-135 VCC
      GPIO1/A1/D1      ┤●    ●├ GND ← MQ-135 GND
      GPIO2/A2/D2      ┤●    ●├ 3V3
      GPIO21/D3        ┤●    ●├ D10/MOSI/GPIO18
      GPIO22/SDA/D4    ┤●    ●├ D9/MISO/GPIO20
      GPIO23/SCL/D5    ┤●    ●├ D8/SCK/GPIO19
      GPIO16/TX/D6     ┤●    ●├ D7/RX/GPIO17
                       └──────┘
GPIO15: 온보드 LED (Active-Low)
```

→ [XIAO ESP32-C6 상세 레퍼런스](docs/xiao-esp32c6.md)

---

## 소프트웨어 아키텍처

### 파일 구조

```
pet-toilet-monitor_v2/
├── main/
│   ├── main.c                    # Zigbee 메인 로직
│   ├── main.h                    # 디바이스 설정, 타이밍 매크로
│   ├── event_detector.c          # 배뇨/배변 이벤트 감지 상태 머신
│   ├── event_detector.h
│   ├── air_sensor_driver_MQ135.c # MQ-135 ADC 드라이버
│   ├── air_sensor_driver.h       # 센서 추상화 헤더
│   ├── light_driver_internal.c   # GPIO15 LED 드라이버 (Active-Low)
│   ├── light_driver.h
│   ├── zcl_utility.c             # ZCL 문자열 등록 유틸리티
│   └── zcl_utility.h
├── litterbox-driver/             # SmartThings Edge Driver
│   ├── config.yaml
│   ├── fingerprints.yaml
│   ├── profiles/litterbox-v1.yaml
│   ├── src/init.lua
│   └── custom-capability/        # NH₃ + 배변 이벤트 커스텀 Capability
├── docs/
│   ├── MQ-135.md                 # 센서 상세 레퍼런스 (배선, 수식, 한계)
│   ├── xiao-esp32c6.md           # 보드 핀아웃, ADC 주의사항
│   └── calibration.md            # R0 캘리브레이션 절차 + 실측 기록
├── build.ps1                     # ESP-IDF 빌드 스크립트 (PowerShell)
├── flash.ps1                     # 플래시 스크립트 (COM3)
├── monitor.ps1                   # 시리얼 모니터 스크립트
├── flash_monitor.ps1             # 플래시 + 모니터 통합
└── PROJECT_PLAN.md               # 단계별 개발 로드맵
```

### Zigbee 클러스터 구성

| 클러스터 | ID | 속성 | 설명 |
|----------|-----|------|------|
| Basic | 0x0000 | - | 제조사(Reasty) / 모델(LitterBox.v1) |
| Identify | 0x0003 | - | 디바이스 식별 |
| On/Off | 0x0006 | - | LED 원격 제어 |
| NH₃ Custom | 0xFC00 | 0x0000: uint16 ppm | NH₃ 농도 (10초 주기) |
| NH₃ Custom | 0xFC00 | 0x0003: uint8 | 이벤트 타입 (변경 시 즉시) |

> Endpoint: **1** (SmartThings는 endpoint 1을 요구함)

### SmartThings 커스텀 Capability

| Capability ID | 속성 | 표시 |
|---------------|------|------|
| `streetsmile37673.nh3measurement` | `ammoniaLevel` (number, ppm) | 암모니아(NH3) |
| `streetsmile37673.toiletevent` | `toiletEvent` (enum: none/urination/defecation) | 배변 감지 |

---

## 빌드 / 플래시 / 모니터

### 환경

- **OS**: Windows 11
- **프레임워크**: ESP-IDF v5.5.2
- **중요**: Git Bash에서는 ESP-IDF export가 동작하지 않음 → **PowerShell 필수**

### 스크립트 사용법

```powershell
# 빌드 (set-target 포함 — 초기 또는 타겟 변경 시)
powershell.exe -ExecutionPolicy Bypass -File build.ps1

# 플래시 (COM3 하드코딩)
powershell.exe -ExecutionPolicy Bypass -File flash.ps1

# 시리얼 모니터
powershell.exe -ExecutionPolicy Bypass -File monitor.ps1

# 플래시 + 모니터 한번에
powershell.exe -ExecutionPolicy Bypass -File flash_monitor.ps1
```

### ESP-IDF CMD에서 직접 사용할 경우

```cmd
idf.py set-target esp32c6
idf.py build
idf.py -p COM3 flash
idf.py -p COM3 monitor
```

### Zigbee NVS 초기화 (클러스터 ID 변경 시 필수)

```powershell
# 클러스터 ID 변경 후 기존 NVS 설정이 충돌하면 크래시 발생 → 반드시 초기화
$esptool = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\esptool.exe"
& $esptool --port COM3 --baud 460800 erase_region 0xF1000 0x5000
```

---

## 학습 내용 요약

### ESP-IDF & Zigbee

ESP32-C6는 802.15.4를 칩 내장으로 지원하여 외부 모듈 없이 Zigbee 3.0 사용 가능.
ESP-IDF + esp-zigbee-sdk 조합으로 Zigbee End Device를 구현했다.

**핵심 API 패턴**:

```c
// Zigbee 스택 초기화 → 네트워크 조인 → 속성 보고 반복
esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
esp_zb_init(&zb_nwk_cfg);

// 커스텀 클러스터에서는 esp_zb_zcl_update_reporting_info() 사용 금지
// → 수동 타이머 + 직접 리포트만 가능
esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_sample_timer_cb, 0, INTERVAL_MS);
esp_zb_zcl_report_attr_cmd_req(&report_cmd);
```

**ZCL 문자열 포맷** (길이 바이트 + 문자열):
```c
#define ESP_MANUFACTURER_NAME "\x06""Reasty"        // 6바이트
#define ESP_MODEL_IDENTIFIER  "\x0c""LitterBox.v1"  // 12바이트
```

### 커스텀 Zigbee 클러스터

ZCL에는 NH₃ 전용 클러스터가 없어 제조사 특정 클러스터(0xFC00)를 직접 정의했다.

**배운 것**:
- 클러스터 ID 0xFC00~0xFFFF 범위가 제조사 특정 영역
- `esp_zb_zcl_update_reporting_info()`는 표준 클러스터에만 사용 가능. 커스텀 클러스터에 사용하면 `zcl_general_commands.c:612` assertion crash 발생
- 해결책: `esp_zb_scheduler_alarm()` 기반 수동 타이머로 주기적 리포트
- **NVS 주의**: 클러스터 ID를 변경하면 NVS에 저장된 이전 리포팅 설정이 남아 부팅 시 크래시. `erase_region`으로 Zigbee NVS 영역 초기화 필수

### SmartThings Edge Driver

SmartThings Hub에서 실행되는 Lua 기반 드라이버. Zigbee 클러스터 → SmartThings Capability 매핑을 담당한다.

**배운 것**:

1. **`config.yaml`의 `permissions: zigbee: {}`는 필수**
   - 이 항목이 없으면 드라이버가 조용히 로딩 실패. 에러 메시지 없음. logcat에 아무것도 안 나옴.

2. **`device_init`에서 초기값 emit 필수**
   - `device_added`는 최초 페어링 시에만 호출됨
   - 드라이버 전환 후 앱에 "-" 표시 방지를 위해 `device_init`에서도 상태 emit 필요

3. **커스텀 Capability 등록** (무료, Samsung 계정 필요):
   ```bash
   smartthings capabilities:create -i schema.json
   smartthings capabilities:presentation:create <capId> <version> -i presentation.json
   smartthings capabilities:translations:upsert <capId> <version> -i translation-ko.json
   ```

4. **enum 번역 포맷** (`i18n.value.<key>` 구조 — 잘못된 형식 사용 시 422 에러):
   ```json
   "i18n": { "value": { "none": "감지 안 됨", "urination": "소변 감지됨" } }
   ```

5. **드라이버 전환 시 반드시 ID 직접 지정**:
   ```bash
   smartthings edge:drivers:switch <deviceId> --hub <hubId> --driver <driverId>
   ```
   대화형 목록은 순서가 바뀔 수 있어 위험

### MQ-135 센서 & 캘리브레이션

MQ-135는 SnO₂ 반도체 산화물 센서로, 가스 농도에 따라 내부 저항(Rs)이 변한다.

**ppm 계산 원리**:
```
ppm = 102.2 × (Rs / R0)^(-2.473)   // NH₃ 멱함수 모델

V_adc       = raw / 4095 × 3.3     // ADC 전압
AOUT_sensor = V_adc × 2.0          // 분배기 역산
Rs          = RL × (VCC − AOUT) / AOUT
```

**R0 캘리브레이션 결과** (2026-02-23):
- 조건: 창문 열린 실내, 30분 예열, 5V + 100kΩ:100kΩ 분배기
- 평균 raw: 953, 측정 R0: **6.3 kΩ**
- clean air 기준 NH₃: ~4~5 ppm

**주요 특성 / 주의사항**:
- 최초 예열 24시간 이상 필요 (이후 재투입: 3~5분)
- 히터 전력 ~800mW — 배터리 운용 부적합
- 교차 민감도: NH₃ 뿐만 아니라 CO₂, 알코올, 벤젠 등에도 반응
- 10ppm 미만 구간에서 감도 급락

→ [상세 캘리브레이션 문서](docs/calibration.md)

### 전압 분배기 설계

5V 센서 출력을 ESP32-C6 ADC 최대 입력(3.1V) 이하로 낮추기 위한 분배기.
100kΩ:100kΩ 선정 이유:

| 제약 | 요구 | 100kΩ 결과 |
|------|------|-----------|
| Loading effect (R2 ≥ 10×RL_internal) | R2 ≥ 100kΩ | 100kΩ ✅ |
| ADC 소스 임피던스 < 100kΩ | R1∥R2 < 100kΩ | 50kΩ ✅ |
| 전류 소비 최소화 | 높을수록 유리 | 25μA ✅ |

→ [자세한 수식 및 설명](docs/MQ-135.md#왜-100kΩ인가----저항값-선정-이유)

---

## 이벤트 감지 알고리즘

NH₃ 농도 변화를 3-state 상태 머신으로 분석하여 배뇨/배변 이벤트를 감지한다.

```
IDLE ──(ppm > baseline + 10)──► ACTIVE ──(3연속 임계 이하)──► COOLDOWN ──(60s)──► IDLE
```

| 상태 | 동작 |
|------|------|
| IDLE | EMA로 baseline 업데이트 중 |
| ACTIVE | 이벤트 진행 중. 피크 ppm / ticks 추적 |
| COOLDOWN | 이벤트 종료 후 60초 대기 |

**이벤트 분류**:
- `peak_ticks ≤ 3` (30초 내 피크) **또는** `peak_ppm - baseline > 30 ppm` → **소변** (급격한 스파이크)
- 그 외 → **대변** (완만한 상승)

**ADC 샘플링**: 2초 주기 / **Zigbee 보고**: 10초 주기 (tick 카운터로 분리)

---

## 실제 테스트 결과 & 한계점

### 1차 실제 소변 테스트 (2026-02-24)

| 항목 | 값 |
|------|-----|
| 센서 위치 | 밀폐형 화장실 모래 바로 앞, 1cm |
| baseline | 4.6 ppm |
| 측정 최대 | 6.3 ppm |
| **delta** | **+1.6 ppm** |
| **트리거 임계값** | **10 ppm** |
| **결과** | **미감지** |

**근본 원인 분석**:

| 원인 | 설명 |
|------|------|
| 응고 모래(벤토나이트)의 NH₃ 흡착 | 소변이 굳으면서 NH₃를 가두어 대기 중 확산이 차단됨 |
| 10ppm 임계값 vs. 1.6ppm 실제 delta | 약 6배 부족 — 임계값을 낮춰도 환경 노이즈와 구분 어려움 |

### NH₃ 단독 감지 방식의 한계

| 한계 | 설명 |
|------|------|
| 응고 모래 흡착 | 소변 직후 NH₃가 모래에 갇혀 신호 극소화 |
| 소변 vs. 대변 가스 프로파일 상이 | 소변: NH₃ / 대변: H₂S, 스카톨, 메르캅탄 → MQ-135 단독으로 구분 어려움 |
| 노령묘 소변량 적음 | 건강 문제로 소변량 감소 → NH₃ 발생량도 더 적음 |

### 검토했던 대안 센서들의 한계

| 방식 | 한계 |
|------|------|
| 습도 센서 | 욕실 근처 환경 → 사람 목욕 시 오감지 |
| 로드셀 (중량) | 5kg 모래 tare 대비 30~80g 감지 → 저가 HX711으로는 S/N비 부족 |
| 모래 속 수분 센서 | 위생 및 내구성 문제 |

### 결론

**암모니아 센서 단독으로 응고 모래 + 밀폐형 화장실에서 소변 이벤트를 안정적으로 감지하기는 어렵다.**
더 적합한 접근법: PIR(고양이 존재 감지) + 습도 또는 중량 센서 조합. 단, 이번 프로젝트 환경에서는 각 센서마다 별도의 제약이 있어 추가 탐색이 필요하다.

---

## 트러블슈팅 기록

### 커스텀 클러스터에서 크래시 루프

**현상**: 클러스터 0xFC00에 `esp_zb_zcl_update_reporting_info()` 사용 시 assertion crash.

```c
// ❌ 커스텀 클러스터에서 crash (zcl_general_commands.c:612)
esp_zb_zcl_update_reporting_info(&reporting_info);

// ✅ 수동 타이머 + 직접 리포트
esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_sample_timer_cb, 0, INTERVAL_MS);
esp_zb_zcl_report_attr_cmd_req(&nh3_report);
```

### SmartThings 앱에서 값이 NaN / null

**원인**: `config.yaml`에 `permissions: zigbee: {}` 누락 → 드라이버 자체가 로딩되지 않음.

```yaml
# config.yaml
permissions:
  zigbee: {}   # 필수! 없으면 드라이버 조용히 실패
```

### 클러스터 ID 변경 후 부팅 크래시

**원인**: NVS에 저장된 이전 클러스터 리포팅 설정 잔존.

```powershell
# 해결: Zigbee NVS 영역 초기화
& $esptool --port COM3 --baud 460800 erase_region 0xF1000 0x5000
```

### 드라이버 전환 후 logcat 출력 없음

**원인**: 반복적인 드라이버 전환으로 Zigbee 디바이스가 허브 연결을 잃음.

**해결**: SmartThings 앱에서 디바이스 삭제 → 재페어링.

### 커스텀 Capability 앱에서 "-" 표시

**원인**: `device_init`에서 초기값 미emit.

```lua
local function device_init(driver, device)
  device:emit_event(toiletEvent.toiletEvent({ value = "none" }))
  device:emit_event(nh3Measurement.ammoniaLevel({ value = 0, unit = "ppm" }))
end
```

---

## 상세 문서

| 문서 | 내용 |
|------|------|
| [docs/MQ-135.md](docs/MQ-135.md) | 센서 원리, 배선 방법 A/B, 전압 분배기 수식, 한계점 |
| [docs/xiao-esp32c6.md](docs/xiao-esp32c6.md) | 보드 핀아웃, ADC 사양, 핀 이름 체계 |
| [docs/calibration.md](docs/calibration.md) | R0 캘리브레이션 절차 + 실측 기록 + 실제 테스트 결과 |
| [PROJECT_PLAN.md](PROJECT_PLAN.md) | 6단계 개발 로드맵 및 진행 현황 |
| [CLAUDE.md](CLAUDE.md) | 개발 환경, 아키텍처, 트러블슈팅 전체 기록 |

---

## 개발 환경

| 항목 | 버전 |
|------|------|
| MCU | Seeed XIAO ESP32-C6 |
| 프레임워크 | ESP-IDF v5.5.2 |
| Zigbee SDK | esp-zigbee-sdk (espressif__esp-zigbee-lib ~1.6.0) |
| OS | Windows 11 |
| IDE | VS Code + ESP-IDF Extension |
| SmartThings Driver | Edge Driver v20 (Lua) |

---

## 라이선스

CC0-1.0

---

_개발 기간: 2026-02 / 최종 업데이트: 2026-02-24_
