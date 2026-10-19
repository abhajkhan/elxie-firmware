@echo off
rem Hardware testbench with the web dashboard: builds + flashes the testbench program,
rem then opens tools\testbench-ui\ElxieTestbench.html in Edge to watch every sensor live.
rem Afterwards run FLASH_HOME_FIRMWARE.bat to put the Home firmware back.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\testbench.ps1" -Ui
echo.
echo Done. You can close this window.
pause
