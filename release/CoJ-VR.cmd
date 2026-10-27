@echo off
setlocal
set "COJVR_PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%COJVR_PS%" (
  echo Windows PowerShell 5.1 is required.
  pause
  exit /b 1
)
if "%~1"=="" (
  "%COJVR_PS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0cojvr-gui.ps1"
) else (
  "%COJVR_PS%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0cojvr.ps1" %*
)
set "COJVR_RESULT=%ERRORLEVEL%"
if not "%COJVR_RESULT%"=="0" pause
exit /b %COJVR_RESULT%
