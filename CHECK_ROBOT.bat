@echo off
rem Robot check: flash chip type/size + 25 s start-up log -> logs\robot_check_<MAC>.log
rem Plug in ONE robot over USB and close serial monitors first.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\check_robot.ps1"
echo.
echo Done. You can close this window.
pause
