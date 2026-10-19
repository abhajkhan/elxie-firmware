# Elxie Block Code - one-click setup + build (written by Claude)
# Logs go to logs\build.log next to this script.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
New-Item -ItemType Directory -Force -Path "$root\logs" | Out-Null
Start-Transcript -Path "$root\logs\build.log" -Force | Out-Null

Write-Host "=== 1. Project files ==="
if (-not (Test-Path "$root\firmware\CMakeLists.txt")) {
  if (Test-Path "$root\elxie-code") { Rename-Item "$root\elxie-code" "_old_arduino_version"; Write-Host "old Arduino copy -> _old_arduino_version" }
  Expand-Archive -Force "$root\ElxieBlockCode-espidf.zip" -DestinationPath "$root\_tmp"
  Get-ChildItem -Force "$root\_tmp\ElxieBlockCode" | ForEach-Object { Move-Item -Force $_.FullName "$root\" }
  Remove-Item -Recurse -Force "$root\_tmp"
  Write-Host "ESP-IDF project unpacked"
} else { Write-Host "ESP-IDF project already present" }

if (Test-Path "$root\firmware-update.zip") {
  Write-Host "applying firmware-update.zip"
  Remove-Item -Force -ErrorAction SilentlyContinue "$root\firmware\main\link\usb_transport.h", "$root\firmware\main\link\usb_transport.cpp", "$root\firmware\dependencies.lock"
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue "$root\firmware\managed_components", "$root\firmware\build", "$root\firmware\sdkconfig"
  Expand-Archive -Force "$root\firmware-update.zip" -DestinationPath "$root"
  Move-Item -Force "$root\firmware-update.zip" "$root\logs\firmware-update.applied.zip"
}

Write-Host "=== 2. Find ESP-IDF ==="
Get-ChildItem -Force C:\Espressif -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
Get-ChildItem -Force C:\Espressif\tools -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
Get-ChildItem -Force C:\esp -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
$activated = $false
$profiles = @(Get-ChildItem C:\Espressif\tools -Filter "Microsoft*PowerShell_profile.ps1" -ErrorAction SilentlyContinue) + @(Get-ChildItem C:\Espressif -Filter "Microsoft*PowerShell_profile.ps1" -ErrorAction SilentlyContinue)
$prof = $profiles | Where-Object { $_.Name -like "*5.5.4*" } | Select-Object -First 1
if (-not $prof) { $prof = $profiles | Select-Object -First 1 }
if ($prof) { Write-Host "activating with $($prof.FullName)"; . $prof.FullName; $activated = $true }
elseif (Test-Path C:\esp\v5.5.4\esp-idf\export.ps1) { Write-Host "activating with export.ps1"; . C:\esp\v5.5.4\esp-idf\export.ps1; $activated = $true }
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) { Write-Host "ERROR: idf.py not found after activation"; Stop-Transcript | Out-Null; exit 1 }
idf.py --version

Write-Host "=== 3. Build ==="
Set-Location "$root\firmware"
# Remove files from earlier versions and regenerate sdkconfig when the defaults changed.
Remove-Item -Force -ErrorAction SilentlyContinue "main\link\usb_transport.h", "main\link\usb_transport.cpp"
if ((Test-Path "sdkconfig") -and ((Get-Item "sdkconfig.defaults").LastWriteTime -gt (Get-Item "sdkconfig").LastWriteTime)) {
  Write-Host "sdkconfig.defaults changed: regenerating sdkconfig"
  Remove-Item -Force "sdkconfig"
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue "build"
}
if (-not (Test-Path "sdkconfig")) { idf.py set-target esp32s3 2>&1 | ForEach-Object { "$_" } | Out-Host }
if (-not (Test-Path "build\build.ninja")) { idf.py reconfigure 2>&1 | ForEach-Object { "$_" } | Out-Host }
# Only 2 compile jobs at a time: the default (one per CPU thread) ran out of RAM on this laptop.
ninja -C build -j 2 -k 0 2>&1 | ForEach-Object { "$_" } | Out-Host
if ($LASTEXITCODE -eq 0) { Write-Host "BUILD_RESULT=OK" } else { Write-Host "BUILD_RESULT=FAILED ($LASTEXITCODE)" }
Stop-Transcript | Out-Null
