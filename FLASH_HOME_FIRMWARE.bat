@echo off
rem Builds and flashes the Elxie HOME firmware (games + Blocks). Plug Elxie in and close serial monitors first.
rem Log: logs\home_flash.log
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\home_firmware.ps1" -Action flash
echo.
echo Done. You can close this window.
pause
