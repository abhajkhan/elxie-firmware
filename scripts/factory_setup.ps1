# Factory station: prepares one robot + remote set for the pilot, step by step, and
# records it in logs\factory_record.csv. Used by FACTORY_SETUP.bat.
#   1. (optional) flash the remote          - plug in only the REMOTE
#   2. flash the robot + boot check          - plug in only the ROBOT
#   3. pair the robot with its remote
#   4. compass calibration (robot spins 6 s)
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)  # scripts\ -> repo root
$logs = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$csv = Join-Path $logs "factory_record.csv"
$self = Join-Path $root "scripts"

function Say($text, $color = "White") { Write-Host $text -ForegroundColor $color }
function Step($n, $text) { Write-Host ""; Write-Host "=== $n. $text ===" -ForegroundColor Cyan }
function YesNo($q, $default) {
    $hint = if ($default) { "[Y/n]" } else { "[y/N]" }
    $a = Read-Host "$q $hint"
    if ([string]::IsNullOrWhiteSpace($a)) { return $default }
    return $a.Trim().ToLower().StartsWith("y")
}
function RunScript($file, $arguments) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $self $file) @arguments | Out-Host
    return $LASTEXITCODE
}

# ---------------------------------------------------------------- record
$rec = [ordered]@{
    date = (Get-Date -Format "yyyy-MM-dd HH:mm"); set = ""; robot_mac = ""; remote_mac = "";
    remote_flashed = "skipped"; robot_flashed = "skipped"; boot = "?"; compass_chip = "?";
    paired = "no"; compass_cal = "no"; result = "INCOMPLETE"; notes = ""
}
function Save {
    $line = ($rec.Values | ForEach-Object { '"' + ("$_" -replace '"', "'") + '"' }) -join ","
    if (-not (Test-Path $csv)) { (($rec.Keys | ForEach-Object { $_ }) -join ",") | Out-File -FilePath $csv -Encoding utf8 }
    $line | Out-File -FilePath $csv -Encoding utf8 -Append
}

Say "Elxie factory station: one robot + one remote per run." "Yellow"
Say "Have ready: the robot (battery charged), its remote (batteries in), a USB cable, and clear floor space." "Yellow"
$rec.set = Read-Host "Set number written on the robot and remote (e.g. 1..10)"

# ---------------------------------------------------------------- 1. remote
Step 1 "Remote firmware"
if (YesNo "Flash this set's REMOTE now? (needed once, for the new pairing-only firmware)" $true) {
    Say "Plug in ONLY the remote over USB (unplug the robot)." "Yellow"
    Read-Host "Press Enter when the remote is plugged in"
    if ((RunScript "remote_tools.ps1" @("-Action", "firmware")) -eq 0) { $rec.remote_flashed = "ok" }
    else { $rec.remote_flashed = "FAILED"; Say "Remote flash failed. See logs\remote_firmware.log" "Red" }
    Say "Unplug the remote. It can stay switched off for now." "Yellow"
}

# ---------------------------------------------------------------- 2. robot
Step 2 "Robot firmware"
Say "Plug in ONLY the robot over USB and switch its battery on." "Yellow"
Read-Host "Press Enter when the robot is plugged in"
if (YesNo "Flash the Home firmware onto the robot?" $true) {
    if ((RunScript "home_firmware.ps1" @("-Action", "flash")) -eq 0) { $rec.robot_flashed = "ok" }
    else { $rec.robot_flashed = "FAILED"; Say "Robot flash failed. See logs\home_flash.log" "Red"; Save; exit 1 }
    $bootLog = Join-Path $logs "home_flash.log"
    $rec.boot = if ((Test-Path $bootLog) -and (Select-String -Path $bootLog -Pattern "\[Blocks\] ready" -Quiet)) { "ok" } else { "CHECK" }
    if ($rec.boot -ne "ok") { Say "The boot log has no '[Blocks] ready' line. Run CHECK_ROBOT.bat on this robot." "Red" }
}

# ---------------------------------------------------------------- serial to the robot
$named = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match "\((COM\d+)\)" } | ForEach-Object {
    $null = $_.Name -match "\((COM\d+)\)"; [pscustomobject]@{ Port = $Matches[1]; Name = $_.Name } })
$usb = @($named | Where-Object { $_.Name -match "CH34|CP210|USB.?SERIAL|USB.?UART|USB.?Enhanced|JTAG" })
$ports = if ($usb.Count) { $usb.Port } else { [System.IO.Ports.SerialPort]::GetPortNames() }
if (@($ports).Count -eq 0) { Say "No COM port found. Plug the ROBOT in over USB." "Red"; $rec.notes = "no COM port"; Save; exit 1 }
$port = @($ports | Sort-Object { [int]($_ -replace "\D", "") })[-1]
$sp = New-Object System.IO.Ports.SerialPort $port, 115200
$sp.DtrEnable = $false; $sp.RtsEnable = $false; $sp.ReadTimeout = 300; $sp.NewLine = "`n"
try { $sp.Open() } catch { Say "Could not open $port (close serial monitors / dashboards): $_" "Red"; $rec.notes = "port busy"; Save; exit 1 }
Start-Sleep -Milliseconds 300
$null = $sp.ReadExisting()

function ReadUntil($pattern, $seconds) {
    $end = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $end) {
        try { $line = $sp.ReadLine().TrimEnd("`r") } catch [System.TimeoutException] { continue }
        if ($line -match "^(PAIR|REMOTE|MAGCAL|COMPASS|BUSY)") { Write-Host "  $line" -ForegroundColor DarkCyan }
        if ($line -match $pattern) { return $line }
    }
    return $null
}

$st = $null
for ($i = 0; $i -lt 5 -and -not $st; $i++) { $sp.WriteLine("remote?"); $st = ReadUntil "^REMOTE " 2 }
if (-not $st) { Say "The robot does not answer. Is the Home firmware on it?" "Red"; $sp.Close(); $rec.notes = "no answer"; Save; exit 1 }
# REMOTE <remote mac|none> <paired|unpaired|default> robot <robot mac>
$parts = $st -split "\s+"
$rec.robot_mac = $parts[4]
$alreadyPaired = $parts[2] -eq "paired"
if ($alreadyPaired) { $rec.remote_mac = $parts[1] }
if ($rec.boot -eq "?") { $rec.boot = "ok" }  # answered over serial
$sp.WriteLine("compass?")
$cs = ReadUntil "^COMPASS " 3
$rec.compass_chip = if ($cs -match "COMPASS found") { "ok" } else { "MISSING" }
$compassDone = $cs -match " calibrated "
Say "Robot $($rec.robot_mac): remote $(if ($alreadyPaired) { $rec.remote_mac } else { 'not paired' }), compass chip $($rec.compass_chip)$(if ($compassDone) { ', already calibrated' })"

# ---------------------------------------------------------------- 3. pairing
Step 3 "Pair the robot with remote set $($rec.set)"
$doPair = if ($alreadyPaired) { YesNo "Already paired with $($rec.remote_mac). Pair again?" $false } else { $true }
if ($doPair) {
    Say "Keep every OTHER remote switched off." "Yellow"
    $sp.WriteLine("pair")
    $null = ReadUntil "^PAIR START" 3
    Say "Now on remote $($rec.set): hold FRONT + BOTTOM and switch it on (30 s)." "Yellow"
    $res = ReadUntil "^PAIR (OK|TIMEOUT|CANCELLED)" 40
    if ($res -match "^PAIR OK remote (\S+)") { $rec.remote_mac = $Matches[1]; $rec.paired = "ok"; Say "Paired with remote $($rec.remote_mac)" "Green" }
    else { $rec.paired = "FAILED"; Say "No remote answered. Run FACTORY_SETUP.bat again for this set." "Red" }
} else { $rec.paired = "ok (earlier)" }

# ---------------------------------------------------------------- 4. compass
Step 4 "Compass calibration"
$doCal = if ($rec.compass_chip -ne "ok") { $false } elseif ($compassDone) { YesNo "Compass already calibrated. Calibrate again?" $false } else { $true }
if ($rec.compass_chip -ne "ok") { Say "No compass chip found: skipping. Check the compass board." "Red"; $rec.compass_cal = "NO CHIP" }
elseif ($doCal) {
    Say "Put the robot on a flat floor, 1 m from metal, magnets and laptops. Keep the USB cable loose." "Yellow"
    Read-Host "Press Enter to start (the robot spins for 6 s)"
    $sp.WriteLine("calmag")
    $res = ReadUntil "^MAGCAL (OK|FAILED|CANCELLED)" 15
    if ($res -match "^MAGCAL OK") { $rec.compass_cal = "ok"; Say "Compass calibrated and saved." "Green" }
    else { $rec.compass_cal = "FAILED"; Say "Compass calibration failed. Move away from metal and run CALIBRATE_COMPASS.bat." "Red" }
} else { $rec.compass_cal = "ok (earlier)" }
$sp.Close()

# ---------------------------------------------------------------- result
$okFlash = $rec.robot_flashed -ne "FAILED" -and $rec.remote_flashed -ne "FAILED"
$ok = $okFlash -and $rec.boot -eq "ok" -and $rec.paired -like "ok*" -and $rec.compass_cal -like "ok*"
$rec.result = if ($ok) { "READY" } else { "NOT READY" }
Save
Write-Host ""
$rec.GetEnumerator() | ForEach-Object { "{0,-15} {1}" -f $_.Key, $_.Value } | Out-Host
Write-Host ""
if ($ok) { Say "FACTORY_RESULT=READY  set $($rec.set) saved to logs\factory_record.csv. Label robot and remote '$($rec.set)'." "Green"; exit 0 }
Say "FACTORY_RESULT=NOT READY  (see the lines above; the attempt is saved in logs\factory_record.csv)" "Red"
exit 1
