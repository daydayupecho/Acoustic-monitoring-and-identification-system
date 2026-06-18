@echo off
setlocal

rem ===== user settings =====
REM set "ROOT=E:\SPEECH_DATA\TENG\touming-dataset\arouse-20260411-ODR16000-1s\other"
REM set "OUT_ALL=E:\SPEECH_DATA\TENG\touming-dataset\arouse-20260411-ODR16000-1s\other-14336-14336-multiline-fixed"
set "ROOT=E:\SPEECH_DATA\TENG\touming-dataset\arouse-20260423\other"
set "OUT_ALL=E:\SPEECH_DATA\TENG\touming-dataset\arouse-20260423\other-8192-8192"

set "PYTHON_EXE=python"
set "CLI=%~dp0custom_dat_to_nanoedge_multiline_fixed.py"
set "SENSOR=imp23absu_mic"
set "SL=8192"
set "SI=8192"
set "ODR=16000"
set "CSV_SHAPE=row"
rem To export physical values instead of raw int16, uncomment next line:
rem set "SCALED=--scaled"
rem =========================

echo [INFO] PYTHON_EXE=%PYTHON_EXE%
echo [INFO] CLI=%CLI%
echo [INFO] ROOT=%ROOT%
echo [INFO] OUT_ALL=%OUT_ALL%
echo [INFO] SENSOR=%SENSOR%
echo [INFO] SL=%SL%
echo [INFO] SI=%SI%
echo [INFO] ODR=%ODR%
echo [INFO] CSV_SHAPE=%CSV_SHAPE%

if not exist "%OUT_ALL%" mkdir "%OUT_ALL%"

"%PYTHON_EXE%" "%CLI%" "%ROOT%" -o "%OUT_ALL%" -s "%SENSOR%" -sl %SL% -si %SI% --odr %ODR% --csv-shape %CSV_SHAPE% %SCALED%

if errorlevel 1 (
    echo [ERR ] convert failed
) else (
    echo [DONE] outputs in: %OUT_ALL%
)

pause
