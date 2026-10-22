# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Build the game and its launcher and zip them for release.
.DESCRIPTION
Builds the windows-cabinet preset, which links SDL, Beetle PCE and the C
runtime into the executables, so the zip needs no DLLs or redistributable.
Then zips what CMake installs for a player (the game, the launcher, LICENSE,
README.md and THIRD_PARTY_NOTICES.md) into out/devils-crush-windows-x86_64.zip.
The ROM is never included. A player puts theirs beside the executables.

The launcher's icon and heading are drawn from Devil's Crush.pce at the
repository root when it is there. Without it the launcher is built plain, and
the script warns.
.EXAMPLE
.\tools\Build-Release.ps1
.EXAMPLE
.\tools\Build-Release.ps1 -Clean
#>
[CmdletBinding()]
param(
    # Delete the windows-cabinet build first.
    [switch] $Clean,
    # Zip what is already built.
    [switch] $SkipBuild,
    [string] $CMakePath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$name = 'devils-crush'
$targets = @('devils_crush', 'devils_crush_launcher')
$files = @('devils-crush.exe', 'devils-crush-launcher.exe', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md')

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repositoryRoot 'out/build/windows-cabinet'
$stage = Join-Path $repositoryRoot 'out/release'
$zip = Join-Path $repositoryRoot "out/$name-windows-x86_64.zip"

if (-not $CMakePath) {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { $CMakePath = $command.Source }
    else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $found = @(& $vswhere -latest -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
            if ($found.Count) { $CMakePath = $found[0] }
        }
    }
}
if (-not $CMakePath) { throw 'CMake not found. Install the Visual Studio CMake tools or supply -CMakePath.' }
$CMakePath = (Get-Command $CMakePath -ErrorAction Stop).Source

Push-Location -LiteralPath $repositoryRoot
try {
    if ($Clean -and (Test-Path -LiteralPath $buildRoot)) { Remove-Item -LiteralPath $buildRoot -Recurse -Force }
    if (-not $SkipBuild) {
        & $CMakePath --preset windows-cabinet
        if ($LASTEXITCODE -ne 0) { throw "Configure failed ($LASTEXITCODE)." }
        & $CMakePath --build --preset windows-cabinet --target @targets --parallel
        if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
    }

    # A player gets only the zip, so it must not need SDL3.dll or the
    # Visual C++ redistributable.
    $cache = Join-Path $buildRoot 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache) -or
        -not (Select-String -LiteralPath $cache -Pattern '^CMAKE_MSVC_RUNTIME_LIBRARY:.*=MultiThreaded$' -Quiet) -or
        -not (Select-String -LiteralPath $cache -Pattern '^SDL_STATIC:BOOL=ON$' -Quiet) -or
        -not (Select-String -LiteralPath $cache -Pattern '^SDL_SHARED:BOOL=OFF$' -Quiet)) {
        throw 'A static windows-cabinet build is required. Run without -SkipBuild.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $buildRoot "$name-launcher.ico"))) {
        Write-Warning "No ROM at the repository root when the launcher was built: it has no icon or logo."
    }

    # The stage is made afresh, so the zip holds this build's files and no others.
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    & $CMakePath --install $buildRoot --config Release --component Runtime --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw "Install failed ($LASTEXITCODE)." }
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $file))) { throw "Install did not produce $file." }
    }

    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    Push-Location -LiteralPath $stage
    try {
        & $CMakePath -E tar cf $zip --format=zip @files
        if ($LASTEXITCODE -ne 0) { throw "Zip failed ($LASTEXITCODE)." }
    } finally { Pop-Location }
} finally {
    Pop-Location
}

Write-Host "`nRelease: $zip" -ForegroundColor Green
Get-ChildItem -LiteralPath $stage -File | Sort-Object Name |
    ForEach-Object { '  {0,-28} {1,12:N0} bytes' -f $_.Name, $_.Length } | Write-Host
