param([switch]$Ui)  # -Ui: after flashing, open the web dashboard instead of the console log
# Elxie hardware testbench: build + flash the testbench, then show and save its live log.
# Build/flash output: logs\testbench_build.log   Test log: logs\testbench_<date>_<time>.log
# Afterwards run FLASH_HOME_FIRMWARE.bat to put the Home firmware back.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)  # scripts\ -> repo root
Set-Location $root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
Start-Transcript -Path "$root\logs\testbench_build.log" -Force | Out-Null

Write-Host "=== 1. ESP-IDF ===" -ForegroundColor Cyan
$profiles = @(Get-ChildItem C:\Espressif\tools -Filter "Microsoft*PowerShell_profile.ps1" -ErrorAction SilentlyContinue) + @(Get-ChildItem C:\Espressif -Filter "Microsoft*PowerShell_profile.ps1" -ErrorAction SilentlyContinue)
$prof = $profiles | Where-Object { $_.Name -like "*5.5.4*" } | Select-Object -First 1
if (-not $prof) { $prof = $profiles | Select-Object -First 1 }
if ($prof) { . $prof.FullName }
elseif (Test-Path C:\esp\v5.5.4\esp-idf\export.ps1) { . C:\esp\v5.5.4\esp-idf\export.ps1 }
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) { Write-Host "TESTBENCH_RESULT=NO_IDF (idf.py not found)" -ForegroundColor Red; Stop-Transcript | Out-Null; exit 1 }

Write-Host "=== 2. Build testbench ===" -ForegroundColor Cyan
Set-Location "$root\testbench"
if (-not (Test-Path "sdkconfig")) { idf.py set-target esp32s3 2>&1 | ForEach-Object { "$_" } | Out-Host }
if (-not (Test-Path "build\build.ninja")) { idf.py reconfigure 2>&1 | ForEach-Object { "$_" } | Out-Host }
ninja -C build -j 2 -k 0 2>&1 | ForEach-Object { "$_" } | Out-Host
if ($LASTEXITCODE -ne 0) { Write-Host "TESTBENCH_RESULT=BUILD_FAILED" -ForegroundColor Red; Stop-Transcript | Out-Null; exit 1 }

Write-Host "=== 3. Flash testbench ===" -ForegroundColor Cyan
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports.Count -eq 0) { Write-Host "TESTBENCH_RESULT=NO_PORT (plug Elxie in)" -ForegroundColor Red; Stop-Transcript | Out-Null; exit 1 }
$port = @($ports | Sort-Object)[-1]
if ($ports.Count -gt 1) { Write-Host "several ports: $($ports -join ', ') - using $port" }
idf.py -p $port -b 460800 flash 2>&1 | ForEach-Object { "$_" } | Out-Host
if ($LASTEXITCODE -ne 0) {
  Write-Host "TESTBENCH_RESULT=FLASH_FAILED (is $port busy? close the Test Console / serial monitors)" -ForegroundColor Red
  Stop-Transcript | Out-Null; exit 1
}
Stop-Transcript | Out-Null
Set-Location $root

if ($Ui) {
  Write-Host ""
  Write-Host "TESTBENCH_RESULT=FLASHED  opening the dashboard..." -ForegroundColor Cyan
  Write-Host "In the page: press Connect Elxie, pick $port, then press Restart Elxie or Run all tests." -ForegroundColor Yellow
  Start-Process msedge "`"$root\tools\testbench-ui\ElxieTestbench.html`""
  exit 0
}

# ------------------------------------------------------------------ live log
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logPath = "$root\logs\testbench_$stamp.log"
$log = New-Object System.IO.StreamWriter($logPath, $false, [System.Text.Encoding]::UTF8)
$log.AutoFlush = $true
Write-Host ""
Write-Host "=== 4. Live test log ($port, 115200) -> logs\testbench_$stamp.log ===" -ForegroundColor Cyan
Write-Host "Follow the >>> instructions. Lift the wheels before the motor test." -ForegroundColor Yellow
Write-Host ""

function Show($line) {
  $c = "Gray"
  if ($line -match "^RESULT\s+\S+\s+PASS") { $c = "Green" }
  elseif ($line -match "^RESULT\s+\S+\s+FAIL" -or $line -match "^\s+FAIL ") { $c = "Red" }
  elseif ($line -match "^RESULT\s+\S+\s+WARN" -or $line -match "^\s+WARN ") { $c = "Yellow" }
  elseif ($line -match "^RESULT\s+\S+\s+CHECK" -or $line -match "^\s+CHECK ") { $c = "Magenta" }
  elseif ($line -match "^\s+PASS ") { $c = "Green" }
  elseif ($line -match "^>>>") { $c = "Yellow" }
  elseif ($line -match "^\[\d\d\]|^===|^SUMMARY|^TESTBENCH_") { $c = "Cyan" }
  elseif ($line -match "^E \(|STUCK|no echo") { $c = "Red" }
  Write-Host $line -ForegroundColor $c
}

$sp = New-Object System.IO.Ports.SerialPort $port, 115200
$sp.ReadTimeout = 500
$sp.DtrEnable = $false; $sp.RtsEnable = $false
try { $sp.Open() } catch { Write-Host "Could not open $port : $_" -ForegroundColor Red; $log.Close(); exit 1 }
# restart Elxie so the log starts from the first test (RTS -> EN, like esptool's hard reset)
$sp.RtsEnable = $true; Start-Sleep -Milliseconds 120; $sp.RtsEnable = $false

$t0 = Get-Date
$done = $false
$deadline = (Get-Date).AddSeconds(300)
while (-not $done -and (Get-Date) -lt $deadline) {
  try { $line = $sp.ReadLine().TrimEnd("`r") } catch [System.TimeoutException] { continue }
  if ($line.StartsWith("@")) { continue }  # machine lines for the web dashboard
  $t = ((Get-Date) - $t0).TotalSeconds
  $log.WriteLine(("[{0,6:0.0}s] {1}" -f $t, $line))
  Show $line
  if ($line -match "^TESTBENCH_DONE") { $done = $true }
}
Start-Sleep -Milliseconds 300
try { $rest = $sp.ReadExisting(); if ($rest) { $log.Write($rest); Write-Host $rest } } catch {}
$sp.Close()
$log.Close()

Write-Host ""
if ($done) { Write-Host "TESTBENCH_RESULT=FINISHED  log: logs\testbench_$stamp.log" -ForegroundColor Cyan }
else { Write-Host "TESTBENCH_RESULT=TIMEOUT (no TESTBENCH_DONE in 5 min)  log: logs\testbench_$stamp.log" -ForegroundColor Red }
Write-Host "The testbench stays on Elxie until you run FLASH_HOME_FIRMWARE.bat (hold the touch pad 2 s to re-run the tests)."
