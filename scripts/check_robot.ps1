# Robot check: reads the ESP32-S3 flash chip (type + size) and records a 25 s start-up log.
# Used by CHECK_ROBOT.bat. Saves logs\robot_check_<MAC>.log
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)  # scripts\ -> repo root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
$tmp = "$root\logs\robot_check_current.log"
Start-Transcript -Path $tmp -Force | Out-Null
function Done($code) { Stop-Transcript | Out-Null; exit $code }

$py = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\python.exe"
$pkg = Join-Path $env:USERPROFILE ".platformio\packages\tool-esptoolpy"
$esptool = Join-Path $pkg "esptool.py"
if (-not (Test-Path $py) -or -not (Test-Path $pkg)) {
    Write-Host "PlatformIO tools not found. Run BUILD_HOME_FIRMWARE.bat once first." -ForegroundColor Red; Done 1
}

$named = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match "\((COM\d+)\)" } | ForEach-Object {
    $null = $_.Name -match "\((COM\d+)\)"; [pscustomobject]@{ Port = $Matches[1]; Name = $_.Name } })
$named | ForEach-Object { "$($_.Port)  $($_.Name)" }
$usb = @($named | Where-Object { $_.Name -match "CH34|CP210|USB.?SERIAL|USB.?UART|USB.?Enhanced|JTAG" })
$ports = if ($usb.Count) { $usb.Port } else { [System.IO.Ports.SerialPort]::GetPortNames() }
if (@($ports).Count -eq 0) { Write-Host "No COM port found. Plug the robot in." -ForegroundColor Red; Done 1 }
$port = @($ports | Sort-Object { [int]($_ -replace "\D", "") })[-1]

Write-Host "=== 1. Flash chip ($port) ===" -ForegroundColor Cyan
if (Test-Path $esptool) { $out = & $py $esptool --chip esp32s3 -p $port -b 115200 flash_id 2>&1 | ForEach-Object { "$_" } }
else { $env:PYTHONPATH = $pkg; $out = & $py -m esptool --chip esp32s3 -p $port -b 115200 flash_id 2>&1 | ForEach-Object { "$_" } }
$out | Out-Host
$mac = (($out | Select-String "MAC: ") -replace ".*MAC: ", "" -replace ":", "").Trim()
if (-not $mac) { $mac = "unknown" }

Write-Host "=== 2. Start-up log (25 s) ===" -ForegroundColor Cyan
Start-Sleep -Milliseconds 500
try {
    $sp = New-Object System.IO.Ports.SerialPort $port, 115200
    $sp.DtrEnable = $false; $sp.RtsEnable = $false
    $sp.Open()
    $sp.RtsEnable = $true; Start-Sleep -Milliseconds 120; $sp.RtsEnable = $false  # reset the robot
    $end = (Get-Date).AddSeconds(25)
    while ((Get-Date) -lt $end) { Start-Sleep -Milliseconds 250; $t = $sp.ReadExisting(); if ($t) { Write-Host -NoNewline $t } }
    $sp.Close()
} catch { Write-Host "Could not open $port : $_" -ForegroundColor Red }
Write-Host ""
Write-Host "CHECK_DONE robot $mac" -ForegroundColor Cyan
Stop-Transcript | Out-Null
Copy-Item $tmp "$root\logs\robot_check_$mac.log" -Force
Write-Host "Saved logs\robot_check_$mac.log"
