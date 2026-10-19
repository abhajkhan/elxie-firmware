@echo off
rem Builds the Elxie HOME firmware (games + Blocks) with PlatformIO. Installs PlatformIO the first time.
rem Log: logs\home_build.log
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\home_firmware.ps1" -Action build
echo.
echo Done. You can close this window.
pause
