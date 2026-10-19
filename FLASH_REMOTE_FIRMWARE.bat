@echo off
rem Puts the normal firmware (REMOTE_espnow.ino) back on the hand remote.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\remote_tools.ps1" -Action firmware
echo.
echo Done. You can close this window.
pause
