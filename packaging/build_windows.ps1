#Requires -Version 5.1
<#
.SYNOPSIS
    Build a Windows distribution for GPC Sequence Recorder.

.DESCRIPTION
    Creates packaging/out/gpc-recorder_<version>_win64.zip containing:
      gpc-recorder.exe          - web UI (http://127.0.0.1:8765/)
      gpc-recorder-repl.exe     - stdin REPL
      repo/                     - firmware tree (schema, config CMake project)
      bin/                      - placeholder for host tools (when available)
      README.txt

    Requires on build host: Python 3.10+, pip, cmake (optional, for end-user export builds)

.PARAMETER Version
    Package version (default: read from versions.hpp).

.PARAMETER Help
    Show usage.

.EXAMPLE
    .\packaging\build_windows.ps1
#>
param(
    [string]$Version = "",
    [string]$FwLlcRoot = "",
    [switch]$Help
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..")).Path
if (-not $FwLlcRoot) {
    $FwLlcRoot = Join-Path (Split-Path $RepoRoot -Parent) "fw-llc"
}
$FwLlcCommon = Join-Path $FwLlcRoot "scripts\common"
$OutDir = Join-Path $RepoRoot "packaging\out"
$ToolDir = Join-Path $RepoRoot "tools\gpc_sequence_recorder"
$DistName = "gpc-recorder"
$BuildVenv = Join-Path $RepoRoot "packaging\.build-venv-win"
$SpecFile = Join-Path $ScriptDir "gpc-recorder.spec"

function Show-Usage {
    Write-Host @"
Usage: packaging\build_windows.ps1 [OPTIONS]

Build a Windows zip for GPC recorder tooling (sequence recorder web UI + REPL).

Options:
  -Version VER   Package version (default: from versions.hpp)
  -Help          Show this help

Requires: Python 3.10+ with pip, MinGW g++ (MSYS2/UCRT64 or mingw-w64)

Output: packaging\out\gpc-recorder_<version>_win64.zip

Bundled host tools: g474.exe (programmer), gpc_usb_bluelink.exe (USB bridge).
CAN flashing is Linux-only; Windows uses USB (--transport usb).
"@
}

function Read-VersionFromHeader {
    $header = Join-Path $RepoRoot "versions.hpp"
    if (-not (Test-Path $header)) {
        throw "Missing versions.hpp at $header"
    }
    $text = Get-Content $header -Raw
    $major = if ($text -match 'CONFIG_READ_ONLY_MEMORY_VERSION_MAJOR\s+(\d+)U') { $Matches[1] } else { "0" }
    $minor = if ($text -match 'CONFIG_READ_ONLY_MEMORY_VERSION_MINOR\s+(\d+)U') { $Matches[1] } else { "0" }
    $patch = if ($text -match 'CONFIG_READ_ONLY_MEMORY_VERSION_PATCH\s+(\d+)U') { $Matches[1] } else { "0" }
    return "$major.$minor.$patch"
}

function Copy-RepoTree {
    param(
        [string]$DestRoot
    )

    $paths = @(
        "build_all.sh",
        "CMakeLists.txt",
        "CMakePresets.json",
        "versions.hpp",
        "cmake",
        "configs",
        "interfaces",
        "config_projects\config_g474",
        "tools\gpc_sequence_recorder",
        "3rd_party\bluelink_sdk\bluelink_messages",
        "3rd_party\bluelink_sdk\bluelink_serializer",
        "3rd_party\bluelink_sdk\bluelink_version",
        "3rd_party\bluelink_sdk\scripts"
    )

    $excludeDirs = @(".venv", ".pytest_cache", "__pycache__", "build", "Debug", "Release", ".git")
    $excludeFiles = @("*.pyc")

    New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null

    foreach ($rel in $paths) {
        $src = Join-Path $RepoRoot $rel
        if (-not (Test-Path $src)) {
            throw "Missing expected path: $src"
        }
        $destParent = Join-Path $DestRoot (Split-Path $rel -Parent)
        New-Item -ItemType Directory -Force -Path $destParent | Out-Null
        $dest = Join-Path $DestRoot $rel

        if (Test-Path $src -PathType Container) {
            robocopy $src $dest /E /NFL /NDL /NJH /NJS /NC /NS /NP `
                /XD $excludeDirs `
                /XF $excludeFiles | Out-Null
            if ($LASTEXITCODE -ge 8) {
                throw "robocopy failed for $rel (exit $LASTEXITCODE)"
            }
        } else {
            Copy-Item -Force $src $dest
        }
    }
}

function Write-DistReadme {
    param(
        [string]$Path,
        [string]$PkgVersion
    )

    @"
GPC Sequence Recorder $PkgVersion (Windows)
==========================================

Quick start
-----------
1. Unzip anywhere (e.g. C:\Tools\gpc-recorder\).
2. Double-click gpc-recorder.exe or run from a terminal.
3. Open http://127.0.0.1:8765/ in your browser.

Commands
--------
  gpc-recorder.exe              Web UI + terminal REPL in browser
  gpc-recorder-repl.exe         Stdin REPL (no browser)
  gpc-recorder.exe --port 8766  Use another TCP port

User data
---------
Exports and builds are stored under:
  %LOCALAPPDATA%\gpc-recorder\

Override with environment variable GPC_RECORDER_DATA.

Config bin export
-----------------
export() in the REPL writes g474_gpc_config_memory.hpp and builds config_g474.bin.
Requires arm-none-eabi-gcc on PATH (STM32CubeIDE bundled GNU Tools, or
https://developer.arm.com/downloads/-/gnu-rm) and cmake.

USB / flash
-----------
Host tools in bin\:
  g474.exe / prog-gpc-g4.exe   Flash config/app over USB
  gpc_usb_bluelink.exe         USB bluelink bridge for live commands

Default serial port is COM3; pick your device in the recorder UI or pass -p COMx.
CAN transport is not available on Windows (use USB).

Layout
------
  gpc-recorder.exe, gpc-recorder-repl.exe  - recorder applications
  repo\                                    - firmware tree (read-only)
  bin\                                     - g474.exe, gpc_usb_bluelink.exe
"@ | Set-Content -Path $Path -Encoding UTF8
}

function Build-HostTools {
    param(
        [string]$BinDest
    )

    if (-not (Test-Path $FwLlcCommon)) {
        throw "fw-llc not found at $FwLlcRoot (need scripts\common for serial/win). Clone fw-llc next to this repo or pass -FwLlcRoot."
    }

    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if (-not $gpp) {
        throw "g++ not found. Install MinGW-w64 (e.g. MSYS2: pacman -S mingw-w64-ucrt-x86_64-gcc make)."
    }

    $make = Get-Command mingw32-make -ErrorAction SilentlyContinue
    if (-not $make) {
        $make = Get-Command make -ErrorAction SilentlyContinue
    }
    if (-not $make) {
        throw "make not found. Install make (e.g. MSYS2: pacman -S make)."
    }

    $makeExe = $make.Name
    $llcCommonUnix = ($FwLlcCommon -replace '\\', '/')

    Write-Host "Building g474.exe (programmer)..."
    Push-Location (Join-Path $RepoRoot "programmer\g474")
    try {
        & $makeExe clean
        & $makeExe "CXX=g++" "LLC_COMMON=$llcCommonUnix" all
        Copy-Item -Force "g474.exe" (Join-Path $BinDest "g474.exe")
        Copy-Item -Force "g474.exe" (Join-Path $BinDest "prog-gpc-g4.exe")
    } finally {
        Pop-Location
    }

    Write-Host "Building gpc_usb_bluelink.exe..."
    Push-Location (Join-Path $RepoRoot "tools\gpc_usb_bluelink")
    try {
        & $makeExe clean
        & $makeExe "CC=g++" "LLC_COMMON=$llcCommonUnix" all
        Copy-Item -Force "gpc_usb_bluelink.exe" (Join-Path $BinDest "gpc_usb_bluelink.exe")
    } finally {
        Pop-Location
    }
}

if ($Help) {
    Show-Usage
    exit 0
}

if (-not $Version) {
    $Version = Read-VersionFromHeader
}

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
    if ($python) {
        $pythonExe = "py -3"
    } else {
        throw "Python not found. Install Python 3.10+ and ensure python or py is on PATH."
    }
} else {
    $pythonExe = "python"
}

Write-Host "Building $DistName $Version for win64..."
Write-Host "  repo root: $RepoRoot"

if (-not (Test-Path $BuildVenv)) {
    Write-Host "Creating build venv at $BuildVenv..."
    & $pythonExe -m venv $BuildVenv
}

$venvPython = Join-Path $BuildVenv "Scripts\python.exe"
$venvPip = Join-Path $BuildVenv "Scripts\pip.exe"

& $venvPip install -q --upgrade pip
& $venvPip install -q -r (Join-Path $ToolDir "requirements.txt")
& $venvPip install -q pyinstaller

$pyinstaller = Join-Path $BuildVenv "Scripts\pyinstaller.exe"
Push-Location $RepoRoot
try {
    & $pyinstaller --noconfirm --clean $SpecFile
} finally {
    Pop-Location
}

$distDir = Join-Path $RepoRoot "dist\$DistName"
if (-not (Test-Path $distDir)) {
    throw "PyInstaller output not found at $distDir"
}

$repoDest = Join-Path $distDir "repo"
$binDest = Join-Path $distDir "bin"
Write-Host "Staging firmware tree to $repoDest..."
if (Test-Path $repoDest) {
    Remove-Item -Recurse -Force $repoDest
}
Copy-RepoTree -DestRoot $repoDest

New-Item -ItemType Directory -Force -Path $binDest | Out-Null
Build-HostTools -BinDest $binDest

Write-DistReadme -Path (Join-Path $distDir "README.txt") -PkgVersion $Version

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$zipBase = Join-Path $OutDir "${DistName}_${Version}_win64"
$zipPath = "$zipBase.zip"
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

Write-Host "Creating $zipPath..."
Compress-Archive -Path $distDir -DestinationPath $zipPath -Force

Write-Host ""
Write-Host "Created: $zipPath"
Write-Host "Unzip and run gpc-recorder.exe (opens http://127.0.0.1:8765/)"
