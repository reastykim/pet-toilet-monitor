# CLAUDE_old.md - 초기 계획 문서 (아카이브)

> 이 파일은 프로젝트 초기에 작성된 계획 문서를 보관용으로 유지합니다.
> 현재 프로젝트 가이드는 `CLAUDE.md`를 참조하세요.

---

## 빌드 환경 설정 기록 (2026-02-18 검증 완료)

### ESP-IDF 설치 경로

| 항목 | 경로 |
|------|------|
| IDF_PATH | `C:\Espressif\frameworks\esp-idf-v5.5.2` |
| IDF_TOOLS_PATH | `C:\Espressif` |
| Python venv | `C:\Espressif\python_env\idf5.5_py3.11_env` (Python 3.11.2) |
| cmake | `C:\Espressif\tools\cmake\3.30.2\bin` |
| ninja | `C:\Espressif\tools\ninja\1.12.1` |
| git | `C:\Espressif\tools\idf-git\2.44.0\cmd` |
| RISC-V 툴체인 | `C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20251107\riscv32-esp-elf\bin` |

### 필수 환경 변수

```
IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.2
IDF_TOOLS_PATH=C:\Espressif
IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.5_py3.11_env
MSYSTEM=           (빈 문자열로 설정 - MSys 경고 방지)
PYTHONNOUSERSITE=True
```

### 빌드 방법

Git Bash(MSYS)에서는 ESP-IDF export 스크립트가 동작하지 않음. **PowerShell** 사용 필수:

```powershell
powershell.exe -ExecutionPolicy Bypass -File "D:\00Projects\ESP32\pet-toilet-monitor_v2\build.ps1"
```

또는 ESP-IDF Windows CMD Prompt에서:
```cmd
idf.py set-target esp32c6
idf.py build
idf.py -p COMx flash monitor
```

### 빌드 결과 (Phase 2 최초 빌드)

- **바이너리**: `build/litterbox_v1.bin` (473KB, 파티션 49% 사용)
- **부트로더**: `build/bootloader/bootloader.bin` (31% 사용)
- **컴파일 스텝**: 1060/1060 성공 (오류 없음)
- **타겟**: esp32c6

### 주의사항 (초기 계획과의 차이)

| 항목 | 초기 계획 (틀림) | 실제 (확인됨) |
|------|------------------|---------------|
| ESP-IDF 버전 | v5.3.2 언급 | **v5.5.2** |
| 개발 환경 | Docker 기반 | **로컬 Windows 설치** |
| Python | py3.13 (export 기본) | **py3.11** (idf5.5_py3.11_env) |
| cmake | 3.30.5 | **3.30.2** |
| 센서 읽기 주기 | 100ms | **10초 (10000ms)** |
| NH3 임계값 | 500ppm 고정 | **기준선 대비 +20ppm 상승** |
| 파일명 | light_driver.c | **light_driver_internal.c** |

---

## 원본 초기 계획 (요약)

- ESP32-C6 + Zigbee 3.0 + SmartThings 연동
- MQ-135 센서로 암모니아 감지
- HA_on_off_light 예제 기반 시작
- 센서별 모듈화 설계
- 제조사: Reasty, 모델: LitterBox.v1

_Archived: 2026-02-18_
