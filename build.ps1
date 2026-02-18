$ErrorActionPreference = "Continue"
$logFile = "D:\00Projects\ESP32\pet-toilet-monitor_v2\build_output.log"

"Build started at $(Get-Date)" | Out-File $logFile

$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.2"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:PYTHONNOUSERSITE = "True"
$env:PYTHONHOME = ""
$env:PYTHONPATH = ""
$env:MSYSTEM = ""
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"

$IDF_PYTHON = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

# Build PATH with all ESP-IDF tools
$toolPaths = @(
    "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts"
    "C:\Espressif\tools\cmake\3.30.2\bin"
    "C:\Espressif\tools\ninja\1.12.1"
    "C:\Espressif\tools\idf-git\2.44.0\cmd"
    "$env:IDF_PATH\tools"
)

# Add RISC-V toolchain paths
Get-ChildItem "C:\Espressif\tools\riscv32-esp-elf" -Directory | ForEach-Object {
    $toolPaths += "$($_.FullName)\riscv32-esp-elf\bin"
}
Get-ChildItem "C:\Espressif\tools\riscv32-esp-elf-gdb" -Directory | ForEach-Object {
    $toolPaths += "$($_.FullName)\riscv32-esp-elf-gdb\bin"
}
Get-ChildItem "C:\Espressif\tools\esp-rom-elfs" -Directory | ForEach-Object {
    $toolPaths += $_.FullName
}

$env:PATH = ($toolPaths -join ";") + ";" + $env:PATH

Set-Location "D:\00Projects\ESP32\pet-toilet-monitor_v2"

"=== Checking tools ===" | Tee-Object $logFile -Append
& $IDF_PYTHON --version 2>&1 | Tee-Object $logFile -Append
cmake --version 2>&1 | Select-Object -First 1 | Tee-Object $logFile -Append
ninja --version 2>&1 | Tee-Object $logFile -Append

"=== Setting target to esp32c6 ===" | Tee-Object $logFile -Append
& $IDF_PYTHON "$env:IDF_PATH\tools\idf.py" set-target esp32c6 2>&1 | Tee-Object $logFile -Append
if ($LASTEXITCODE -ne 0) {
    "ERROR: set-target failed with code $LASTEXITCODE" | Tee-Object $logFile -Append
    exit 1
}

"=== Building project ===" | Tee-Object $logFile -Append
& $IDF_PYTHON "$env:IDF_PATH\tools\idf.py" build 2>&1 | Tee-Object $logFile -Append
if ($LASTEXITCODE -ne 0) {
    "ERROR: build failed with code $LASTEXITCODE" | Tee-Object $logFile -Append
    exit 1
}

"=== Build completed successfully ===" | Tee-Object $logFile -Append
