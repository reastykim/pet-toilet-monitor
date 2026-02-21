# LitterBox.v1 - 스마트 반려동물 화장실 모니터 (Smart Pet Toilet Monitor)

## 프로젝트 개요

Zigbee 기반 스마트 반려동물 화장실 모니터링 시스템. MQ-135 센서로 암모니아를 감지하여 SmartThings Hub를 통해 배뇨/배변 이벤트를 알린다.

- **제조사**: Reasty
- **모델**: LitterBox.v1
- **프로토콜**: Zigbee 3.0
- **타겟 플랫폼**: SmartThings
- **MCU**: Seeed XIAO ESP32-C6

---

## 빌드 환경

### 필수 조건

- **OS**: Windows 11
- **프레임워크**: ESP-IDF v5.5.2
- **Python**: 3.11.2 (`C:\Espressif\python_env\idf5.5_py3.11_env`)
- **cmake**: 3.30.2, **ninja**: 1.12.1
- **타겟**: esp32c6

### 빌드 방법

Git Bash에서는 ESP-IDF export가 동작하지 않음. **PowerShell 사용**:

```powershell
powershell.exe -ExecutionPolicy Bypass -File build.ps1
```

ESP-IDF CMD Prompt에서는:
```cmd
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

### 필수 환경 변수

```
IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.2
IDF_TOOLS_PATH=C:\Espressif
IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.5_py3.11_env
MSYSTEM=           (빈 문자열 - MSys 경고 방지)
```

---

## 프로젝트 구조

```
pet-toilet-monitor_v2/
├── main/
│   ├── main.c                    # Zigbee 메인 로직
│   ├── main.h                    # 디바이스 설정, 매크로 정의
│   ├── event_detector.c          # 배뇨/배변 이벤트 감지 상태 머신
│   ├── event_detector.h          # 이벤트 감지기 타입/API
│   ├── light_driver_internal.c   # GPIO15 LED 드라이버 (Active-Low)
│   ├── light_driver.h            # LED 인터페이스
│   ├── air_sensor_driver_MQ135.c # MQ-135 ADC 센서 드라이버
│   ├── air_sensor_driver.h       # 센서 추상화 헤더
│   ├── zcl_utility.c             # ZCL 유틸리티 (제조사 정보 등록)
│   ├── zcl_utility.h             # ZCL 유틸리티 헤더
│   ├── idf_component.yml         # 컴포넌트 의존성
│   └── CMakeLists.txt            # 소스 파일 등록
├── CMakeLists.txt                # 프로젝트 레벨 빌드 (litterbox_v1)
├── partitions.csv                # 파티션 테이블
├── sdkconfig.defaults            # Zigbee ZED 기본 설정
├── build.ps1                     # PowerShell 빌드 스크립트
├── CLAUDE.md                     # 이 파일
├── litterbox-driver/             # SmartThings Edge Driver
│   ├── config.yaml               # 드라이버 메타데이터 (packageKey, permissions)
│   ├── fingerprints.yaml         # Reasty/LitterBox.v1 핑거프린트
│   ├── profiles/
│   │   └── litterbox-v1.yaml     # 디바이스 프로파일 (커스텀 capabilities)
│   ├── src/
│   │   └── init.lua              # Zigbee → SmartThings 매핑 로직
│   └── custom-capability/        # SmartThings 커스텀 Capability 정의 파일
│       ├── nh3Measurement.json                  # NH₃ 농도 capability 스키마
│       ├── nh3Measurement-presentation.json     # 앱 표시 설정
│       ├── nh3Measurement-translation-ko.json   # 한국어 번역
│       ├── nh3Measurement-translation-en.json   # 영문 번역
│       ├── toiletEvent.json                     # 배변 이벤트 capability 스키마
│       ├── toiletEvent-presentation.json        # 앱 표시 설정 (3상태)
│       ├── toiletEvent-translation-ko.json      # 한국어 번역
│       └── toiletEvent-translation-en.json      # 영문 번역
├── CLAUDE_old.md                 # 초기 계획 아카이브 + 빌드 설정 기록
└── PROJECT_PLAN.md               # 6단계 로드맵
```

---

## 현재 구현 상태 (Phase 6 완료 + 커스텀 Capability 적용)

### Phase 1~6 (완료)

- [x] 프로젝트 독립화 (예제에서 분리, EXTRA_COMPONENT_DIRS 제거)
- [x] GPIO15 LED 드라이버 (Active-Low, GPIO 직접 제어)
- [x] 디바이스 정보: Reasty / LitterBox.v1 (ZCL 문자열 포맷)
- [x] SmartThings Hub 페어링 성공 (Endpoint 1)
- [x] 커스텀 NH₃ 클러스터 (0xFC00, uint16 ppm) 구현
- [x] On/Off 클러스터 유지 (LED 원격 제어)
- [x] **[Phase 5]** MQ-135 ADC 통합 (R0=10.0kΩ 임시, 5V 재캘리브레이션 필요, 10초 주기 리포트)
- [x] **[Phase 6]** 배뇨/배변 이벤트 감지 상태 머신 (IDLE/ACTIVE/COOLDOWN)
- [x] attr 0x0003 (uint8): 이벤트 타입 전송 (0=없음, 1=소변, 2=대변)
- [x] SmartThings 커스텀 Capability 2종 등록 및 적용 (v20 드라이버)

### 미완료 (다음 Phase)

- [ ] **[Phase 7]** 전력 관리 (Deep Sleep)

---

## 하드웨어 사양

### XIAO ESP32-C6

- **내장 LED**: GPIO15, Active-Low (LOW = ON, HIGH = OFF)
- **Zigbee**: 802.15.4 네이티브 지원 (별도 모듈 불필요)
- **ADC**: GPIO0 (ADC1_CH0) ← MQ-135 AOUT (100kΩ:100kΩ 분배기 경유)

### MQ-135 Air Quality Sensor

- **인터페이스**: 아날로그 출력 → 100kΩ:100kΩ 분배기 → GPIO0 (ADC1_CH0)
- **측정 대상**: NH₃, 벤젠, 알코올, 연기
- **용도**: 암모니아 농도 변화로 배뇨/배변 감지
- **모니터링 주기**: 10초
- **전원**: VCC = **5V (VBUS)** — 3.3V 구동 시 감도 부족 확인 (2026-02-22)
- **배선**: AOUT → 100kΩ → GPIO0, 분기점 100kΩ → GND (최대 2.5V ADC 입력)
- **R0**: 10.0 kΩ (임시값, 5V 환경 재캘리브레이션 필요)

---

## Zigbee 클러스터 구성

| 클러스터 | ID | 역할 | 속성 | 용도 |
|----------|--------|--------|------|------|
| Basic | 0x0000 | Server | - | 제조사/모델 정보 |
| Identify | 0x0003 | Server | - | 디바이스 식별 |
| NH₃ Custom | 0xFC00 | Server | 0x0000: uint16 ppm | NH₃ 농도 (10초 주기) |
| NH₃ Custom | 0xFC00 | Server | 0x0003: uint8 | 이벤트 타입 (변경 시만) |
| On/Off | 0x0006 | Server | - | LED 원격 제어 |

- **Endpoint**: 1 (`HA_LITTERBOX_ENDPOINT`) — SmartThings는 endpoint 1을 기대함
- **Device ID**: `ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID`
- **ZCL 문자열 포맷**: 길이 바이트 + 문자열 (예: `"\x06""Reasty"`)

---

## SmartThings Edge Driver

### 현재 버전: v20

`litterbox-driver/` 디렉토리에 위치.

#### 배포 명령

```bash
# 패키징 (litterbox-driver/ 디렉토리에서)
smartthings edge:drivers:package

# 채널 할당 + 허브 설치
smartthings edge:channels:assign <driverId> --channel <channelId>
smartthings edge:drivers:install <driverId> --channel <channelId> --hub <hubId>

# 드라이버 전환 (ID 직접 지정 권장)
smartthings edge:drivers:switch <deviceId> --hub <hubId> --driver <driverId>

# 로그 확인
smartthings edge:drivers:logcat <driverId> --hub-address <hubIP>

# 드라이버 삭제
smartthings edge:drivers:uninstall <driverId> --hub <hubId>
```

#### 현재 SmartThings ID

- **Device**: `123c3d8e-4b69-412f-8c21-9ac9987b179d`
- **Driver (v20)**: `1c6ba2e5-4a10-4d77-91f3-d345a6fb0625`
- **Channel**: `30e3213f-f924-4f19-964c-fe44c2a09496`
- **Hub**: `7a93da67-817b-4a20-ad71-f46e023a1992` (IP: `192.168.10.60`)

---

## SmartThings 커스텀 Capability

### 개요

빌트인 capability(tvocMeasurement, carbonMonoxideDetector 등)는 앱의 information 팝업 제목/설명이 고정되어 있어 커스터마이징 불가. 커스텀 capability를 생성하면 제목, 설명, enum 표시 텍스트를 자유롭게 설정할 수 있다.

- **생성 비용**: 무료 (Samsung 계정만 필요)
- **제한**: Works with SmartThings(WWST) 공식 인증 불가 (개인 DIY 프로젝트에는 무관)
- **Namespace**: `streetsmile37673` (계정당 1개 자동 생성)

### 등록된 커스텀 Capability

#### 1. `streetsmile37673.nh3measurement` — 암모니아 농도

| 항목 | 값 |
|------|-----|
| Capability ID | `streetsmile37673.nh3measurement` |
| Version | 1 |
| Attribute | `ammoniaLevel` (number, 0~1000 ppm) |
| 앱 표시 레이블 | 암모니아(NH3) |
| 앱 information 팝업 | 제목: 암모니아(NH3) / 내용: NH3 - 암모니아 |
| 표시 타입 | slider (0~1000 ppm) + 기간별 차트 |

**Lua 사용 예시**:
```lua
local nh3Measurement = capabilities["streetsmile37673.nh3measurement"]
device:emit_event(nh3Measurement.ammoniaLevel({ value = ppm, unit = "ppm" }))
```

#### 2. `streetsmile37673.toiletevent` — 배변 감지

| 항목 | 값 |
|------|-----|
| Capability ID | `streetsmile37673.toiletevent` |
| Version | 1 |
| Attribute | `toiletEvent` (enum: none / urination / defecation) |
| 앱 표시 레이블 | 배변 감지 |
| 앱 information 팝업 | 제목: 배변 감지 / 내용: 배뇨/배변 이벤트 감지 |
| idle 상태 | 감지 안 됨 |
| 소변 감지 | 소변 감지됨 |
| 대변 감지 | 대변 감지됨 |

**Lua 사용 예시**:
```lua
local toiletEvent = capabilities["streetsmile37673.toiletevent"]
device:emit_event(toiletEvent.toiletEvent({ value = "none" }))       -- 평상시
device:emit_event(toiletEvent.toiletEvent({ value = "urination" }))  -- 소변
device:emit_event(toiletEvent.toiletEvent({ value = "defecation" })) -- 대변
```

### 커스텀 Capability 등록 절차

새 capability를 만들어야 할 때:

```bash
# 1. Capability 생성 (스키마 정의 JSON 필요)
smartthings capabilities:create -i <schema.json> -j

# 2. Presentation 생성 (앱 표시 방식 정의)
smartthings capabilities:presentation:create <capId> <version> -i <presentation.json> -j

# 3. 번역 등록 (한국어/영어)
#    enum 타입의 값 번역은 i18n.value 하위에 map으로 작성
smartthings capabilities:translations:upsert <capId> <version> -i <translation-ko.json> -j
smartthings capabilities:translations:upsert <capId> <version> -i <translation-en.json> -j

# 4. Namespace 확인
smartthings capabilities:namespaces
```

**번역 파일 포맷 (enum 속성)**:
```json
{
  "tag": "ko",
  "label": "배변 감지",
  "description": "배뇨/배변 이벤트 감지",
  "attributes": {
    "toiletEvent": {
      "label": "배변 감지",
      "description": "화장실 사용 감지",
      "displayTemplate": "{{device.label}} 상태: {{value}}",
      "i18n": {
        "value": {
          "none": "감지 안 됨",
          "urination": "소변 감지됨",
          "defecation": "대변 감지됨"
        }
      }
    }
  },
  "commands": {}
}
```

> **주의**: enum 값의 번역 키는 `i18n.value.<enumKey>` 구조. `i18n.<enumKey>.label` (잘못된 형식) 이나 `"attributeName.enumKey"` 플랫 구조(잘못된 형식) 모두 422 에러 발생.

### 커스텀 Capability 적용 (Edge Driver)

**profiles/litterbox-v1.yaml**:
```yaml
components:
  - id: main
    capabilities:
      - id: streetsmile37673.nh3measurement
        version: 1
      - id: streetsmile37673.toiletevent
        version: 1
```

**init.lua**:
```lua
local nh3Measurement = capabilities["streetsmile37673.nh3measurement"]
local toiletEvent    = capabilities["streetsmile37673.toiletevent"]
```

> **주의**: `device_init`에서도 초기값을 emit해야 드라이버 전환 후 앱에 "-" 대신 정상 값이 표시됨.
> `device_added`는 최초 페어링 시에만 호출되고, 드라이버 전환 시에는 `device_init`만 호출됨.

---

## 이벤트 감지 알고리즘 (Phase 6)

### 3-state 상태 머신

```
IDLE ──(ppm > baseline + 10)──► ACTIVE ──(3연속 임계 이하)──► COOLDOWN ──(60s)──► IDLE
```

| 상태 | 설명 |
|------|------|
| IDLE | 평상시. EMA 기저선 업데이트 중 |
| ACTIVE | 이벤트 진행 중. 피크/ticks 추적 |
| COOLDOWN | 이벤트 종료 후 60초 대기 |

### 이벤트 분류 기준

- `peak_ticks ≤ 3` (30초 내 피크) **또는** `peak_ppm - baseline > 30 ppm` → **소변** (급격한 스파이크)
- 그 외 → **대변** (완만한 상승)

### 주요 상수 (`event_detector.h`)

```c
#define EVENT_TRIGGER_DELTA_PPM   10.0f  // 기저선 대비 상승 임계값
#define EVENT_HYSTERESIS_PPM       3.0f  // 종료 판정 여유값
#define EVENT_END_TICKS            3     // 연속 3회(30s) 임계 이하 → 종료
#define EVENT_COOLDOWN_TICKS       6     // 종료 후 60s 쿨다운
#define URINE_FAST_PEAK_TICKS      3     // 30s 내 피크 → 소변 판정
#define BASELINE_ALPHA             0.05f // EMA 업데이트 계수 (~200초 시정수)
```

---

## 트러블슈팅 기록

### 문제: SmartThings 앱에서 온도값이 NaN / null 표시

**근본 원인**: `config.yaml`에 `permissions: zigbee: {}` 누락

SmartThings Edge Driver의 `config.yaml`에 Zigbee 권한 선언이 없으면, 허브가 **드라이버를 아예 시작하지 않는다**. 에러 메시지도 없이 조용히 실패하므로 발견이 매우 어렵다.

```yaml
permissions:
  zigbee: {}   # 필수! 없으면 드라이버 로딩 자체 안 됨
```

---

### 문제: 커스텀 클러스터(0xFC00)에서 크래시 루프

**근본 원인 1**: `esp_zb_zcl_update_reporting_info()`를 커스텀 클러스터에 사용하면 ZCL 스택 assertion crash.

```c
/* ❌ 커스텀 클러스터에서 crash */
// esp_zb_zcl_update_reporting_info(&reporting_info);

/* ✅ 수동 타이머 + 직접 리포트 */
esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_report_timer_cb, 0, INTERVAL_MS);
esp_zb_zcl_report_attr_cmd_req(&nh3_report);
```

**근본 원인 2**: NVS에 저장된 이전 클러스터 리포팅 설정이 남아 crash.

```powershell
# 클러스터 ID 변경 시 반드시 Zigbee NVS 초기화
& $esptool --port COM3 --baud 460800 erase_region 0xF1000 0x5000
```

---

### 문제: Edge Driver 전환 후 logcat 출력 없음 / 디바이스 데이터 멈춤

**현상**: `smartthings edge:drivers:switch` 후 logcat에 아무 출력도 없고 NH3 타임스탬프가 업데이트되지 않음.

**원인**: 반복적인 드라이버 전환(특히 잘못된 드라이버로 전환)으로 Zigbee 디바이스가 허브와의 연결을 잃음.

**해결**: SmartThings 앱에서 디바이스 삭제 후 재페어링.

**예방**:
- 드라이버 전환 시 반드시 `--driver <driverId>` 플래그로 ID를 직접 지정
- `echo "숫자" | smartthings edge:drivers:switch` 방식은 목록 순서가 바뀔 수 있어 위험

---

### 문제: 커스텀 Capability 앱에서 "-" 표시

**현상**: 드라이버 전환 후 커스텀 capability 값이 "-"로 표시됨.

**원인**: `device_init`에서 초기값을 emit하지 않아 SmartThings 클라우드에 현재 값이 없음. (`device_added`는 최초 페어링 시에만 호출됨)

**해결**: `device_init`에서도 초기 상태를 emit:
```lua
local function device_init(driver, device)
  local ok, err = pcall(function()
    device:emit_event(toiletEvent.toiletEvent({ value = "none" }))
  end)
  if not ok then log.error("device_init emit failed: " .. tostring(err)) end
end
```

---

## 코드 컨벤션

- ESP-IDF 코딩 스타일 준수
- 센서별 독립 소스 파일 (`light_driver_internal.c`, `air_sensor_driver_MQ135.c`)
- 헤더는 센서 교체 가능하도록 추상화 (`light_driver.h`, `air_sensor_driver.h`)
- 커밋 메시지: 영문, conventional commits (`feat:`, `fix:`, `refactor:`)
- `led_strip` 라이브러리 사용하지 않음 (GPIO 직접 제어)

---

## 참조 예제

- `esp-zigbee-sdk/examples/esp_zigbee_HA_sample/HA_on_off_light`
- `esp-zigbee-sdk/examples/esp_zigbee_HA_sample/HA_temperature_sensor`
- `esp-zigbee-sdk/examples/common/zcl_utility`

로컬 예제 경로: `D:\00Projects\ESP32\esp-zigbee-sdk\examples\`

---

_Last Updated: 2026-02-21 (Phase 6 완료, 커스텀 Capability 2종 적용, Edge Driver v17)_
