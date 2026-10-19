@echo off
rem Hardware testbench: builds + flashes a test program, then shows and saves a live log of
rem every sensor and actuator check (logs\testbench_<date>_<time>.log).
rem Afterwards run FLASH_HOME_FIRMWARE.bat to put the Home firmware back.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\testbench.ps1"
echo.
echo Done. You can close this window.
pause
