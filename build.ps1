# Configure + build multipassword with MSVC (Ninja).
# Usage: .\build.ps1 [release|debug|test|clean]
#
# Environment overrides:
#   VCPKG_ROOT  vcpkg checkout with libsodium installed (default C:\vcpkg)
#   QT_DIR      Qt 6 MSVC kit               (default C:\Qt\6.9.3\msvc2022_64)
#   MSVC_DIR    VC\Tools\MSVC\<version>      (auto-detected)
#   WINSDK_VER  Windows 10/11 SDK version    (auto-detected, newest)
param([string]$Task = "release")
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# --- locate toolchain ------------------------------------------------------
$vsRoots = @(
  "C:\Program Files\Microsoft Visual Studio\18\Community",
  "C:\Program Files\Microsoft Visual Studio\18\Professional",
  "C:\Program Files\Microsoft Visual Studio\18\Enterprise",
  "C:\Program Files\Microsoft Visual Studio\2022\Community",
  "C:\Program Files\Microsoft Visual Studio\2022\Professional",
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise")
$vs = $vsRoots | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vs) { throw "Visual Studio with C++ tools not found." }

if (-not $env:MSVC_DIR) {
  $env:MSVC_DIR = (Get-ChildItem "$vs\VC\Tools\MSVC" | Sort-Object Name -Descending | Select-Object -First 1).FullName
}
$sdkRoot = "C:\Program Files (x86)\Windows Kits\10"
if (-not $env:WINSDK_VER) {
  $env:WINSDK_VER = (Get-ChildItem "$sdkRoot\Include" | Where-Object { Test-Path "$($_.FullName)\um\windows.h" } |
                     Sort-Object Name -Descending | Select-Object -First 1).Name
}
if (-not $env:VCPKG_ROOT -or -not (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) { $env:VCPKG_ROOT = "C:\vcpkg" }
if (-not $env:QT_DIR) { $env:QT_DIR = "C:\Qt\6.9.3\msvc2022_64" }

# --- replicate VsDevCmd (-arch=x64 -host_arch=x64) without vswhere -------------
$msvc = $env:MSVC_DIR
$sdk  = $env:WINSDK_VER
$env:INCLUDE = "$msvc\include;$msvc\atlmfc\include;$sdkRoot\Include\$sdk\ucrt;$sdkRoot\Include\$sdk\um;$sdkRoot\Include\$sdk\shared;$sdkRoot\Include\$sdk\winrt;$sdkRoot\Include\$sdk\cppwinrt"
$env:LIB     = "$msvc\lib\x64;$msvc\atlmfc\lib\x64;$sdkRoot\Lib\$sdk\ucrt\x64;$sdkRoot\Lib\$sdk\um\x64"
$env:LIBPATH = "$msvc\lib\x64;$msvc\atlmfc\lib\x64"
$env:PATH    = "$msvc\bin\Hostx64\x64;$sdkRoot\bin\$sdk\x64;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:QT_DIR\bin;" + $env:PATH
$env:VSCMD_ARG_TGT_ARCH = "x64"
$env:Platform = "x64"

Write-Host "MSVC   : $msvc"
Write-Host "WinSDK : $sdk"
Write-Host "Qt     : $env:QT_DIR"
Write-Host "vcpkg  : $env:VCPKG_ROOT"

function Invoke-Step([string[]]$cmd) {
  & $cmd[0] $cmd[1..($cmd.Length-1)]
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

switch ($Task) {
  "clean"   { Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue }
  "debug"   { Invoke-Step @("cmake","--preset","windows-debug"); Invoke-Step @("cmake","--build","--preset","windows-debug") }
  "test"    { Invoke-Step @("cmake","--preset","windows-release"); Invoke-Step @("cmake","--build","--preset","windows-release","--target","mp_tests"); Invoke-Step @("build\windows-release\mp_tests.exe") }
  default   { Invoke-Step @("cmake","--preset","windows-release"); Invoke-Step @("cmake","--build","--preset","windows-release") }
}
exit 0
