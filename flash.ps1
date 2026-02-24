$ErrorActionPreference = "Continue"

$env:IDF_PATH = "C:\Espressif\frameworks\esp-idf-v5.5.2"
$env:IDF_TOOLS_PATH = "C:\Espressif"
$env:PYTHONNOUSERSITE = "True"
$env:PYTHONHOME = ""
$env:PYTHONPATH = ""
$env:MSYSTEM = ""
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"

$IDF_PYTHON = "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

$toolPaths = @(
    "C:\Espressif\python_env\idf5.5_py3.11_env\Scripts"
    "C:\Espressif\tools\cmake\3.30.2\bin"
    "C:\Espressif\tools\ninja\1.12.1"
    "C:\Espressif\tools\idf-git\2.44.0\cmd"
    "$env:IDF_PATH\tools"
)
Get-ChildItem "C:\Espressif\tools\riscv32-esp-elf" -Directory | ForEach-Object {
    $toolPaths += "$($_.FullName)\riscv32-esp-elf\bin"
}
$env:PATH = ($toolPaths -join ";") + ";" + $env:PATH

Set-Location "D:\00Projects\ESP32\pet-toilet-monitor_v2"

"=== Flashing to COM3 ==="
& $IDF_PYTHON "$env:IDF_PATH\tools\idf.py" -p COM3 flash 2>&1
