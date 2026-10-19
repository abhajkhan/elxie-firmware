@echo off
rem Double-click me: unpacks and builds the Elxie Block Code firmware. Log: logs\build.log
rem NOTE: this is the OLD standalone Block Code firmware (reference only). For the robots use
rem FLASH_HOME_FIRMWARE.bat in the repo root (games + Blocks + pairing).
echo This is the old standalone Block Code firmware, NOT the Home firmware.
echo For the pilot robots use FLASH_HOME_FIRMWARE.bat in the repo root.
choice /M "Continue anyway"
if errorlevel 2 exit /b
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_firmware.ps1"
echo.
echo Done. You can close this window.
pause
