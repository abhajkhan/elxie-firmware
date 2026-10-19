@echo off
rem Compass calibration (factory step, once per robot): plug in the ROBOT (Home firmware)
rem over USB, put it on a flat floor away from metal. It spins for 6 s and saves.
rem Log: logs\robot_checks.log
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\robot_checks.ps1" -Action calmag
echo.
echo Done. You can close this window.
pause
