@echo off
setlocal

set "LOGFILE=D:\00Projects\ESP32\pet-toilet-monitor_v2\build_output.log"
echo Build started at %date% %time% > "%LOGFILE%"

set "IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.2"
set "IDF_TOOLS_PATH=C:\Espressif"
set "PYTHONNOUSERSITE=True"
set "PYTHONHOME="
set "PYTHONPATH="
set "MSYSTEM="

set "IDF_PYTHON=C:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"

set "PATH=C:\Espressif\python_env\idf5.5_py3.11_env\Scripts;C:\Espressif\tools\cmake\3.30.5\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;%IDF_PATH%\tools;%PATH%"

for /d %%D in (C:\Espressif\tools\riscv32-esp-elf\*) do set "PATH=%%D\riscv32-esp-elf\bin;%PATH%"
for /d %%D in (C:\Espressif\tools\riscv32-esp-elf-gdb\*) do set "PATH=%%D\riscv32-esp-elf-gdb\bin;%PATH%"
for /d %%D in (C:\Espressif\tools\esp-rom-elfs\*) do set "PATH=%%D;%PATH%"

cd /d "D:\00Projects\ESP32\pet-toilet-monitor_v2"

echo === Checking tools === >> "%LOGFILE%" 2>&1
where cmake >> "%LOGFILE%" 2>&1
where ninja >> "%LOGFILE%" 2>&1
%IDF_PYTHON% --version >> "%LOGFILE%" 2>&1

echo === Setting target to esp32c6 === >> "%LOGFILE%" 2>&1
%IDF_PYTHON% "%IDF_PATH%\tools\idf.py" set-target esp32c6 >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: set-target failed with code %ERRORLEVEL% >> "%LOGFILE%" 2>&1
    exit /b 1
)

echo === Building project === >> "%LOGFILE%" 2>&1
%IDF_PYTHON% "%IDF_PATH%\tools\idf.py" build >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: build failed with code %ERRORLEVEL% >> "%LOGFILE%" 2>&1
    exit /b 1
)

echo === Build completed successfully === >> "%LOGFILE%" 2>&1
endlocal
