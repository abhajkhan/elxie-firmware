# Factory checks for the robot (Home firmware, on USB):
#   -Action calmag   : compass calibration (the robot spins in place for 6 s and saves)
#   -Action tilttest : guided tilt test, prints the IMU settings for blocks_config.h
# Used by CALIBRATE_COMPASS.bat and TILT_TEST.bat. Log: logs\robot_checks.log
param([ValidateSet("calmag", "tilttest")][string]$Action = "calmag")
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)  # scripts\ -> repo root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
Start-Transcript -Path "$root\logs\robot_checks.log" -Append | Out-Null
function Done($code) { Stop-Transcript | Out-Null; exit $code }

$named = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match "\((COM\d+)\)" } | ForEach-Object {
    $null = $_.Name -match "\((COM\d+)\)"; [pscustomobject]@{ Port = $Matches[1]; Name = $_.Name } })
$usb = @($named | Where-Object { $_.Name -match "CH34|CP210|USB.?SERIAL|USB.?UART|USB.?Enhanced|JTAG" })
$ports = if ($usb.Count) { $usb.Port } else { [System.IO.Ports.SerialPort]::GetPortNames() }
if (@($ports).Count -eq 0) { Write-Host "No COM port found. Plug the ROBOT in over USB." -ForegroundColor Red; Done 1 }
$port = @($ports | Sort-Object { [int]($_ -replace "\D", "") })[-1]
if (@($ports).Count -gt 1) { Write-Host "Several ports ($($ports -join ', ')), using $port. Plug in only the robot." -ForegroundColor Yellow }

$sp = New-Object System.IO.Ports.SerialPort $port, 115200
$sp.DtrEnable = $false; $sp.RtsEnable = $false; $sp.ReadTimeout = 300; $sp.NewLine = "`n"
try { $sp.Open() } catch { Write-Host "Could not open $port (close serial monitors / dashboards): $_" -ForegroundColor Red; Done 1 }
Start-Sleep -Milliseconds 300
$null = $sp.ReadExisting()

function ReadUntil($pattern, $seconds) {
    $end = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $end) {
        try { $line = $sp.ReadLine().TrimEnd("`r") } catch [System.TimeoutException] { continue }
        if ($line -match "^(MAGCAL|COMPASS|TILT|REMOTE|BUSY)") {
            $color = if ($line -match "FAILED|UNCLEAR|CHANGE|BUSY") { "Red" } elseif ($line -match "STEP") { "Yellow" } else { "Cyan" }
            Write-Host $line -ForegroundColor $color
        }
        if ($line -match $pattern) { return $line }
    }
    return $null
}

$sp.WriteLine("remote?")
if (-not (ReadUntil "^REMOTE " 3)) { Write-Host "The robot did not answer. Is the Home firmware (FLASH_HOME_FIRMWARE.bat) on it?" -ForegroundColor Red; $sp.Close(); Done 1 }

if ($Action -eq "calmag") {
    Write-Host ""
    Write-Host "Put the robot on a flat floor, at least 1 m from metal, magnets, speakers and laptops." -ForegroundColor Yellow
    Write-Host "Keep the USB cable loose: the robot spins in place for 6 seconds." -ForegroundColor Yellow
    Read-Host "Press Enter to start"
    $sp.WriteLine("calmag")
    $res = ReadUntil "^MAGCAL (OK|FAILED|CANCELLED)" 15
    $sp.WriteLine("compass?"); $null = ReadUntil "^COMPASS " 3
    $sp.Close()
    if ($res -match "^MAGCAL OK") { Write-Host "COMPASS_RESULT=OK (saved on the robot)" -ForegroundColor Green; Done 0 }
    Write-Host "COMPASS_RESULT=FAILED  (run it again on a flat floor away from metal)" -ForegroundColor Red
    Done 1
}

# tilttest
Write-Host ""
Write-Host "Hold the robot in your hand. Follow the prompts (also shown on the robot's screen):" -ForegroundColor Yellow
Write-Host "  1. level   2. NOSE (front) down about 45 degrees   3. RIGHT side down about 45 degrees" -ForegroundColor Yellow
Write-Host "Each pose is held for 6 seconds." -ForegroundColor Yellow
Read-Host "Press Enter to start"
$sp.WriteLine("tilttest")
$res = ReadUntil "^TILT RESULT" 30
$sp.Close()
if ($res -match "MATCHES") { Write-Host "TILT_RESULT=OK (blocks_config.h is correct)" -ForegroundColor Green; Done 0 }
if ($res -match "CHANGE") { Write-Host "TILT_RESULT=CHANGE  -> send this TILT RESULT line to Claude / put the values in src\blocks\blocks_config.h" -ForegroundColor Yellow; Done 0 }
Write-Host "TILT_RESULT=FAILED  (tip further and hold still, then run again)" -ForegroundColor Red
Done 1
