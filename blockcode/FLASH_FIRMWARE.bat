@echo off
rem Flashes the Elxie Block Code firmware to the robot (plug it in first). Log: logs\flash.log
rem NOTE: this is the OLD standalone Block Code firmware (reference only). For the robots use
rem FLASH_HOME_FIRMWARE.bat in the repo root (games + Blocks + pairing).
echo This is the old standalone Block Code firmware, NOT the Home firmware.
echo For the pilot robots use FLASH_HOME_FIRMWARE.bat in the repo root.
choice /M "Continue anyway"
if errorlevel 2 exit /b
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_firmware.ps1"
echo.
echo Done. You can close this window.
pause
