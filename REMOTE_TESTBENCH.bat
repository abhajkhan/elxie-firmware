@echo off
rem Remote testbench: builds and flashes the testbench onto the hand remote (ESP8266),
rem then opens tools\remote-testbench-ui\RemoteTestbench.html to watch it live.
rem Plug in only the remote. Afterwards run FLASH_REMOTE_FIRMWARE.bat.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\remote_tools.ps1" -Action testbench
echo.
echo Done. You can close this window.
pause
