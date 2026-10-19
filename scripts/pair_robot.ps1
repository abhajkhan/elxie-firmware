# Factory pairing: puts the robot (Home firmware, on USB) into remote-pairing mode and
# shows the result. Used by PAIR_ROBOT_AND_REMOTE.bat. Log: logs\pairing.log
param([ValidateSet("pair", "unpair", "status")][string]$Action = "pair")
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)  # scripts\ -> repo root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
Start-Transcript -Path "$root\logs\pairing.log" -Append | Out-Null
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
try { $sp.Open() } catch { Write-Host "Could not open $port (close serial monitors / the Test Console): $_" -ForegroundColor Red; Done 1 }
Start-Sleep -Milliseconds 300
$null = $sp.ReadExisting()

function ReadUntil($pattern, $seconds) {
    $end = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $end) {
        try { $line = $sp.ReadLine().TrimEnd("`r") } catch [System.TimeoutException] { continue }
        if ($line -match "^(PAIR|REMOTE)") { Write-Host $line -ForegroundColor Cyan }
        if ($line -match $pattern) { return $line }
    }
    return $null
}

$sp.WriteLine("remote?")
$st = ReadUntil "^REMOTE " 3
if (-not $st) { Write-Host "The robot did not answer. Is the Home firmware (FLASH_HOME_FIRMWARE.bat) on it?" -ForegroundColor Red; $sp.Close(); Done 1 }

if ($Action -eq "unpair") {
    $sp.WriteLine("unpair"); $null = ReadUntil "^PAIR FORGOTTEN" 3
    $sp.WriteLine("remote?"); $null = ReadUntil "^REMOTE " 3
    $sp.Close(); Done 0
}
if ($Action -eq "status") { $sp.Close(); Done 0 }

$sp.WriteLine("pair")
Write-Host ""
Write-Host "The robot is now in pairing mode (blue blinking, 30 s)." -ForegroundColor Yellow
Write-Host "On the REMOTE: hold the FRONT + BOTTOM buttons and switch it on." -ForegroundColor Yellow
Write-Host ""
$res = ReadUntil "^PAIR (OK|TIMEOUT|CANCELLED)" 40
$sp.Close()
if ($res -match "^PAIR OK") { Write-Host "PAIRING_RESULT=OK" -ForegroundColor Green; Done 0 }
Write-Host "PAIRING_RESULT=FAILED  (no remote answered. Hold FRONT + BOTTOM while switching the remote on, within 30 s)" -ForegroundColor Red
Done 1
