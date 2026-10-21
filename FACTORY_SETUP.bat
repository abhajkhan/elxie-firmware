@echo off
rem Factory station: prepares one robot + remote set for the pilot (remote firmware,
rem robot firmware, pairing, compass) and records it in logs\factory_record.csv.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\factory_setup.ps1"
echo.
echo Done. You can close this window.
pause
