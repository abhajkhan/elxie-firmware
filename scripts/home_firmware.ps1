# Elxie HOME firmware (games + Blocks): build or flash with PlatformIO.
# Used by BUILD_HOME_FIRMWARE.bat and FLASH_HOME_FIRMWARE.bat. Logs: logs\home_build.log / logs\home_flash.log
param([ValidateSet("build", "flash")][string]$Action = "build")
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)  # scripts\ -> repo root
$proj = $root  # the Home firmware is the PlatformIO project at the repo root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
Start-Transcript -Path "$root\logs\home_$Action.log" -Force | Out-Null

function Done($code) { Stop-Transcript | Out-Null; exit $code }

# ---------------------------------------------------------------- PlatformIO
# Same place the PlatformIO installer and the VS Code extension use, so they share it.
$penv = Join-Path $env:USERPROFILE ".platformio\penv"
$pio = Join-Path $penv "Scripts\pio.exe"

function Find-Python {
    $cands = @()
    if (Get-Command py -ErrorAction SilentlyContinue) { $cands += ,@("py", "-3") }
    foreach ($n in "python", "python3") {
        $c = Get-Command $n -ErrorAction SilentlyContinue
        # skip the Microsoft Store placeholder (it only opens the Store)
        if ($c -and $c.Source -notmatch "WindowsApps") { $cands += ,@($c.Source) }
    }
    # Python that came with ESP-IDF (C:\Espressif), used for the Block Code firmware
    foreach ($pat in "C:\Espressif\tools\idf-python\*\python.exe", "C:\Espressif\python_env\*\Scripts\python.exe",
                     "$env:USERPROFILE\.espressif\tools\idf-python\*\python.exe") {
        Get-ChildItem $pat -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | ForEach-Object { $cands += ,@($_.FullName) }
    }
    foreach ($c in $cands) {
        $exe = $c[0]; $pre = @($c | Select-Object -Skip 1)
        $v = & $exe @pre -c "import sys, venv; print('%d.%d' % sys.version_info[:2])" 2>$null
        if ($LASTEXITCODE -eq 0 -and $v -match "^3\.(\d+)$" -and [int]$Matches[1] -ge 8) { return ,$c }
    }
    return $null
}

if (-not (Test-Path $pio)) {
    Write-Host "=== Installing PlatformIO (first time only, a few minutes) ==="
    $py = Find-Python
    if (-not $py) {
        Write-Host ""
        Write-Host "Python 3 was not found. Install it from https://www.python.org/downloads/"
        Write-Host "(tick 'Add python.exe to PATH'), then run this again."
        Write-Host "HOME_RESULT=NO_PYTHON"
        Done 1
    }
    $exe = $py[0]; $pre = @($py | Select-Object -Skip 1)
    Write-Host "Using Python: $($py -join ' ')"
    & $exe @pre -m venv "$penv"
    & "$penv\Scripts\python.exe" -m pip install --upgrade pip 2>&1 | Out-Host
    & "$penv\Scripts\python.exe" -m pip install platformio 2>&1 | Out-Host
    if (-not (Test-Path $pio)) { Write-Host "HOME_RESULT=PIO_INSTALL_FAILED"; Done 1 }
}
& $pio --version

# ---------------------------------------------------------------- build
Write-Host "=== Building the Home firmware (first build downloads the ESP32 tools, ~10 min) ==="
& $pio run -d "$proj" 2>&1 | ForEach-Object { "$_" } | Out-Host
if ($LASTEXITCODE -ne 0) { Write-Host "HOME_RESULT=BUILD_FAILED"; Done 1 }
Write-Host "HOME_RESULT=BUILD_OK"
if ($Action -eq "build") { Done 0 }

# ---------------------------------------------------------------- flash
Write-Host "=== Serial ports ==="
$named = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match "\((COM\d+)\)" } | ForEach-Object {
    $null = $_.Name -match "\((COM\d+)\)"; [pscustomobject]@{ Port = $Matches[1]; Name = $_.Name } })
$named | ForEach-Object { "$($_.Port)  $($_.Name)" }
$usb = @($named | Where-Object { $_.Name -match "CH34|CP210|USB.?SERIAL|USB.?UART|USB.?Enhanced|JTAG" })
$ports = if ($usb.Count) { $usb.Port } else { [System.IO.Ports.SerialPort]::GetPortNames() }
if (@($ports).Count -eq 0) { Write-Host "No COM port found. Plug Elxie in and switch it on."; Write-Host "HOME_RESULT=NO_PORT"; Done 1 }
$port = @($ports | Sort-Object { [int]($_ -replace "\D", "") })[-1]
if (@($ports).Count -gt 1) { Write-Host "several ports: $($ports -join ', ') - using $port" }

Write-Host "=== Flashing via $port (close the Test Console / serial monitors first) ==="
& $pio run -d "$proj" -t upload --upload-port $port 2>&1 | ForEach-Object { "$_" } | Out-Host
if ($LASTEXITCODE -ne 0) { Write-Host "HOME_RESULT=FLASH_FAILED"; Done 1 }
Write-Host "HOME_RESULT=FLASH_OK"

Write-Host "=== Boot log (12 s; the first boot also formats the program storage) ==="
Start-Sleep -Seconds 1
try {
    $sp = New-Object System.IO.Ports.SerialPort $port, 115200
    $sp.DtrEnable = $false; $sp.RtsEnable = $false
    $sp.Open(); Start-Sleep -Seconds 12; Write-Host $sp.ReadExisting(); $sp.Close()
} catch { Write-Host "Could not open $port for the boot log: $_" }
Done 0
