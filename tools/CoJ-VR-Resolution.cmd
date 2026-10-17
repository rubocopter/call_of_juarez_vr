@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0resolution_profile.ps1" %*
set "COJVR_RESULT=%ERRORLEVEL%"
if not "%COJVR_RESULT%"=="0" pause
exit /b %COJVR_RESULT%
