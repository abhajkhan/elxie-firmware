# Elxie Block Code - flash + quick link test (written by Claude). Log: logs\flash.log
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
Start-Transcript -Path "$root\logs\flash.log" -Force | Out-Null
$prof = Get-ChildItem C:\Espressif\tools -Filter "Microsoft.v5.5.4.PowerShell_profile.ps1" | Select-Object -First 1
. $prof.FullName
Write-Host "=== Serial ports ==="
Get-CimInstance Win32_SerialPort | ForEach-Object { "$($_.DeviceID)  $($_.Name)" }
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match "\(COM\d+\)" } | ForEach-Object { $_.Name }
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports.Count -eq 0) { Write-Host "FLASH_RESULT=NO_PORT"; Stop-Transcript | Out-Null; exit 1 }
$port = @($ports | Sort-Object)[-1]
if ($ports.Count -gt 1) { Write-Host "several ports: $($ports -join ', ') - using $port" }
Write-Host "=== Flashing via $port ==="
Set-Location "$root\firmware"
idf.py -p $port -b 460800 flash 2>&1 | ForEach-Object { "$_" } | Out-Host
if ($LASTEXITCODE -ne 0) { Write-Host "FLASH_RESULT=FAILED"; Stop-Transcript | Out-Null; exit 1 }
Write-Host "FLASH_RESULT=OK"
Write-Host "=== Boot log (8 s) ==="
Start-Sleep -Seconds 1
$sp = New-Object System.IO.Ports.SerialPort $port, 115200
$sp.DtrEnable = $false; $sp.RtsEnable = $false
$sp.Open(); Start-Sleep -Seconds 8; Write-Host $sp.ReadExisting(); $sp.Close()
Write-Host "=== Link test ==="
Set-Location $root
python tools\elxie_cli.py --port $port info 2>&1 | ForEach-Object { "$_" } | Out-Host
python tools\elxie_cli.py --port $port list 2>&1 | ForEach-Object { "$_" } | Out-Host
Stop-Transcript | Out-Null
