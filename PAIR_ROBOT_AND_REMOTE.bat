@echo off
rem Factory pairing: plug in the ROBOT (Home firmware) over USB, run this,
rem then hold FRONT + BOTTOM on the remote while switching it on.
rem Log: logs\pairing.log
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\pair_robot.ps1" -Action pair
echo.
echo Done. You can close this window.
pause
