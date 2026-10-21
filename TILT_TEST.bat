@echo off
rem Tilt test: checks that the Blocks tilt readings (pitch/roll) match the robot.
rem Plug in the ROBOT (Home firmware) over USB and hold it in your hand.
rem Log: logs\robot_checks.log
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\robot_checks.ps1" -Action tilttest
echo.
echo Done. You can close this window.
pause
