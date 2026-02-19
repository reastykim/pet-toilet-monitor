# LitterBox.v1 - Smart Cat Litter Box Monitoring System

## 프로젝트 개요

Zigbee 기반 스마트 고양이 화장실 모니터링 시스템. MQ-135 센서로 암모니아를 감지하여 SmartThings Hub를 통해 배뇨/배변 이벤트를 알린다.

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
│   ├── main.c                    # Zigbee 메인 로직 (Temp + CO₂ + On/Off Light)
│   ├── main.h                    # 디바이스 설정, 매크로 정의
│   ├── light_driver_internal.c   # GPIO15 LED 드라이버 (Active-Low)
│   ├── light_driver.h            # LED 인터페이스
│   ├── zcl_utility.c             # ZCL 유틸리티 (제조사 정보 등록)
│   ├── zcl_utility.h             # ZCL 유틸리티 헤더
│   ├── idf_component.yml         # 컴포넌트 의존성
│   └── CMakeLists.txt            # 소스 파일 등록
├── CMakeLists.txt                # 프로젝트 레벨 빌드 (litterbox_v1)
├── partitions.csv                # 파티션 테이블
├── sdkconfig.defaults            # Zigbee ZED 기본 설정
├── build.ps1                     # PowerShell 빌드 스크립트
├── CLAUDE.md                     # 이 파일
├── litterbox-driver/              # SmartThings Edge Driver
│   ├── config.yaml               # 드라이버 메타데이터
│   ├── fingerprints.yaml         # Reasty/LitterBox.v1 핑거프린트
│   ├── profiles/
│   │   └── litterbox-v1.yaml     # Temp + CO₂ + Switch capabilities
│   └── src/
│       └── init.lua              # Zigbee → SmartThings 매핑 로직
├── CLAUDE_old.md                 # 초기 계획 아카이브 + 빌드 설정 기록
└── PROJECT_PLAN.md               # 6단계 로드맵
```

---

## 현재 구현 상태 (Phase 4 완료 - 커스텀 NH₃ 클러스터 0xFC00 SmartThings 연동 검증)

### Phase 1~4 (완료)

- [x] 프로젝트 독립화 (예제에서 분리, EXTRA_COMPONENT_DIRS 제거)
- [x] GPIO15 LED 드라이버 (Active-Low, WS2812 대신 직접 GPIO 제어)
- [x] 디바이스 정보: Reasty / LitterBox.v1 (ZCL 문자열 포맷)
- [x] SmartThings Hub 페어링 성공 (Endpoint 1)
- [x] CO₂ 클러스터(0x040D)로 더미 NH₃ 50 ppm 전송 검증 (Phase 3)
- [x] On/Off 클러스터 유지 (LED 원격 제어)
- [x] 커스텀 NH₃ 클러스터 (0xFC00, uint16 ppm 직접) 구현
- [x] Edge Driver v12: 0xFC00 핸들러 + tvocMeasurement capability
- [x] SmartThings 앱에서 NH₃ 50 ppm 실시간 표시 (10초 간격)
- [x] tvocMeasurement 그래프 뷰 (1h/24h/31d) 지원 확인
- [x] 디바이스 상태: PROVISIONED (정상)

### 미완료 (다음 Phase)

- [ ] **[Phase 5]** MQ-135 센서 ADC 통합 (air_sensor_driver_MQ135.c)
- [ ] **[Phase 5]** 암모니아 기준선 캘리브레이션
- [ ] **[Phase 6]** 배뇨/배변 이벤트 감지 알고리즘
- [ ] **[Phase 7]** 전력 관리 (Deep Sleep)

---

## 하드웨어 사양

### XIAO ESP32-C6

- **내장 LED**: GPIO15, Active-Low (LOW = ON, HIGH = OFF)
- **Zigbee**: 802.15.4 네이티브 지원 (별도 모듈 불필요)
- **ADC**: MQ-135 센서 연결 예정

### MQ-135 Air Quality Sensor

- **인터페이스**: 아날로그 출력 → ESP32-C6 ADC
- **측정 대상**: NH3, 벤젠, 알코올, 연기
- **용도**: 암모니아 농도 변화로 배뇨 감지
- **모니터링 주기**: 10초

---

## Zigbee 클러스터 구성

### 현재 (Phase 4 완료 상태)

| 클러스터 | ID | 역할 | 용도 |
|----------|--------|--------|------|
| Basic | 0x0000 | Server | 제조사/모델 정보 |
| Identify | 0x0003 | Server | 디바이스 식별 |
| NH₃ Custom | 0xFC00 | Server | NH₃ ppm 직접 표현 (uint16, ppm) |
| On/Off | 0x0006 | Server | LED 원격 제어 |

- **Endpoint**: 1 (`HA_LITTERBOX_ENDPOINT`) - SmartThings는 endpoint 1을 기대함
- **Device ID**: `ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID`
- **ZCL 문자열 포맷**: 길이 바이트 + 문자열 (예: `"\x06""Reasty"`)

### ZCL 단위 변환

```
NH₃ Custom (0xFC00):  uint16 = ppm (직접)          (50 ppm → 50)
```

### SmartThings Edge Driver (v12)

`litterbox-driver/` 디렉토리에 위치. 배포 방법:
```powershell
# 패키징 (litterbox-driver/ 디렉토리에서)
smartthings edge:drivers:package

# 채널 할당 + 허브 설치
smartthings edge:channels:assign <driverId> --channel <channelId>
smartthings edge:drivers:install <driverId> --channel <channelId> --hub <hubId>

# 로그 확인
smartthings edge:drivers:logcat <driverId> --hub-address <hubIP>

# 디바이스 삭제 / 드라이버 제거
smartthings devices:delete <deviceId>
smartthings edge:drivers:uninstall <driverId> --hub <hubId>
```

#### 현재 SmartThings ID
- **Device**: `e53f467e-54fb-434e-9a96-3d18f7ea299e`
- **Driver (v12)**: `45c88770-8bb4-4771-bfb1-2b3641780b7e`
- **Channel**: `30e3213f-f924-4f19-964c-fe44c2a09496`
- **Hub**: `7a93da67-817b-4a20-ad71-f46e023a1992` (IP: `192.168.10.60`)

---

## 트러블슈팅 기록

### 문제: SmartThings 앱에서 온도값이 NaN / null 표시

**증상 (Phase 2 디버깅 중 발견)**:
- 디바이스가 Zigbee 네트워크에 정상 참여
- 펌웨어에서 온도 리포트 전송 시 ZCL Default Response status=0x00 (SUCCESS) 수신
- 그러나 SmartThings 앱에서는 온도가 NaN으로 표시
- Edge Driver logcat 출력이 **완전히 0** (startup 로그조차 없음)
- 디바이스 provisioningState가 "TYPED"에서 "PROVISIONED"로 전환되지 않음
- 커스텀 드라이버뿐 아니라 SmartThings 내장 드라이버(Zigbee Sensor)도 동일 증상

**근본 원인**: `config.yaml`에 `permissions: zigbee: {}` 누락

SmartThings Edge Driver의 `config.yaml`에 Zigbee 권한 선언이 없으면,
허브가 **드라이버를 아예 시작하지 않는다**. 드라이버가 로딩되지 않으므로:
- logcat 출력 0 (init.lua의 첫 줄 `log.info()` 조차 실행 안 됨)
- lifecycle 이벤트(added, init, doConfigure)가 발생하지 않음
- doConfigure가 실행되지 않으므로 provisioningState가 "TYPED"에 영원히 머무름
- 허브가 Zigbee 메시지를 수신해도 드라이버로 라우팅하지 않음

**해결 방법 (3가지 수정 사항)**:

1. **`config.yaml`에 `permissions: zigbee: {}` 추가** (핵심 수정)
   ```yaml
   name: litterbox-driver-v9
   packageKey: litterbox-driver-v9
   permissions:
     zigbee: {}          # 이 줄이 없으면 드라이버가 아예 로딩되지 않음!
   description: SmartThings Edge Driver for LitterBox.v1
   vendorSupportInformation: https://github.com/reasty/litterbox
   ```

2. **Endpoint 10 → 1 변경** (main.h)
   - SmartThings Hub는 endpoint 1을 기대함
   - 모든 동작 중인 Zigbee 디바이스(세탁기, 조명, 센서)가 endpoint 1 사용 확인

3. **`defaults.register_for_default_handlers()` 추가** (init.lua)
   - SmartThings 공식 드라이버 패턴에 따라 기본 핸들러 등록
   - ZCL → SmartThings capability 매핑의 표준 인프라 초기화

**참고한 문서 및 자료**:
- [SmartThings Edge Driver 구조 문서](https://developer.smartthings.com/docs/devices/hub-connected/driver-components-and-structure) - `config.yaml` 필수 필드 확인
- [SmartThings Edge Driver Lifecycle 문서](https://developer.smartthings.com/docs/edge-device-drivers/driver.html) - TYPED → PROVISIONED 전환 조건
- [SmartThings Edge Device 문서](https://developer.smartthings.com/docs/edge-device-drivers/device.html) - provisioningState 설명
- [SmartThings Zigbee Defaults 문서](https://developer.smartthings.com/docs/edge-device-drivers/zigbee/defaults.html) - `register_for_default_handlers` 사용법
- [SmartThingsCommunity/SmartThingsEdgeDrivers GitHub](https://github.com/SmartThingsCommunity/SmartThingsEdgeDrivers) - 공식 드라이버 코드에서 `permissions: zigbee: {}` 패턴 확인
- [SmartThings Community: Edge Drivers logcat/installation 이슈](https://community.smartthings.com/t/edge-drivers-issue-with-the-logcat-and-driver-installation/245215)
- [SmartThings Community: doConfigure lifecycle 이슈](https://community.smartthings.com/t/st-edge-issue-with-the-doconfigure-lifecycle/238064)
- [SmartThings Community: Zigbee fingerprint matching](https://community.smartthings.com/t/edge-zigbee-device-fingerprint-matching/278317)

**디버깅 과정에서 시도한 것들** (v4 ~ v8, 모두 실패):
- Edge Driver 코드를 최소화 (supported_capabilities만 남김) → 실패
- 펌웨어에서 CO₂ 클러스터 제거하여 단순화 → 실패
- Binding 기반 → Direct addressing (coordinator ep1) 전환 → Default Response SUCCESS 확인, but NaN
- SmartThings 내장 드라이버로 전환 → 동일하게 실패
- packageKey 변경으로 새 드라이버 ID 생성 (캐시 회피) → 실패
- 디바이스 삭제 후 재페어링 → 실패

**핵심 교훈**: SmartThings Edge Driver의 `config.yaml`에 `permissions: zigbee: {}`가
없으면 드라이버가 로딩 자체가 안 된다. 에러 메시지도 없이 조용히 실패하므로 발견이 매우 어렵다.

---

### 문제: 커스텀 클러스터(0xFC00)에서 크래시 루프

**증상 (Phase 4 디버깅 중 발견)**:
- 펌웨어 플래시 후 디바이스가 네트워크 참여 직후 크래시 + 재부팅 반복
- `Zigbee stack assertion failed zcl/zcl_general_commands.c:612`
- 스택 덤프에 `0x0000040d` (이전 CO₂ 클러스터 ID) 포함

**근본 원인 1**: `esp_zb_zcl_update_reporting_info()` for custom cluster (0xFC00)

`esp_zb_zcl_update_reporting_info()`를 custom/manufacturer-specific cluster에 사용하면
ZCL 내부 리포팅 타이머가 해당 클러스터를 처리하려다 crash. 커스텀 클러스터는
ZCL 스택에 알려지지 않으므로 지원되지 않는다.

**해결**: `esp_zb_zcl_update_reporting_info()` 완전히 제거. 대신 `esp_zb_scheduler_alarm()`으로
수동 타이머를 만들어 `esp_zb_zcl_report_attr_cmd_req()`를 직접 호출.

```c
/* ❌ 이렇게 하면 custom cluster에서 crash */
// esp_zb_zcl_update_reporting_info(&reporting_info);

/* ✅ 이렇게 수동으로 타이머 + 직접 리포트 */
esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_report_timer_cb, 0, INTERVAL_MS);
// timer callback에서:
esp_zb_zcl_report_attr_cmd_req(&nh3_report);
```

**근본 원인 2**: NVS에 저장된 이전 클러스터(0x040D) 리포팅 설정

Zigbee NVS(`zb_storage` 파티션)는 ZCL 리포팅 설정을 영속 저장한다. 클러스터를 변경하면
이전 클러스터의 설정이 남아있고, 재부팅 시 존재하지 않는 클러스터를 처리하려다 crash.

**해결**: esptool로 Zigbee NVS 파티션 초기화 후 재페어링

```powershell
# partitions.csv에서 zb_storage(0xF1000, 0x5000) + zb_fct(0xF5000) 확인
& $esptool --port COM3 --baud 460800 erase_region 0xF1000 0x5000
# 이후 디바이스가 factory-new 모드로 부팅 → SmartThings 앱에서 재페어링
```

**핵심 교훈**:
1. **커스텀 클러스터에 `esp_zb_zcl_update_reporting_info()` 절대 사용 금지** - crash 유발
2. **클러스터 ID 변경 시 반드시 Zigbee NVS 초기화** - 잔류 설정이 crash 유발
3. **재페어링 후 SmartThings가 fingerprint 불일치로 엉뚱한 드라이버 할당할 수 있음** → `smartthings edge:drivers:switch --include-non-matching` 사용

---

## 코드 컨벤션

- ESP-IDF 코딩 스타일 준수
- 센서별 독립 소스 파일 (`light_driver_internal.c`, `air_sensor_driver_MQ135.c`)
- 헤더는 센서 교체 가능하도록 추상화 (`light_driver.h`, `air_sensor_driver.h`)
- 커밋 메시지: 영문, conventional commits (`feat:`, `fix:`, `refactor:`)
- `led_strip` 라이브러리 사용하지 않음 (GPIO 직접 제어)

---

## 참조 예제

본 프로젝트의 기반 코드:
- `esp-zigbee-sdk/examples/esp_zigbee_HA_sample/HA_on_off_light` (Zigbee On/Off Light)
- `esp-zigbee-sdk/examples/esp_zigbee_HA_sample/HA_temperature_sensor` (Temperature 클러스터 패턴)
- `esp-zigbee-sdk/examples/common/zcl_utility` (제조사 정보 유틸리티)

로컬 예제 경로: `D:\00Projects\ESP32\esp-zigbee-sdk\examples\`

---

_Last Updated: 2026-02-19 (Phase 4 완료, 커스텀 NH₃ 클러스터 0xFC00 SmartThings 연동 검증)_
