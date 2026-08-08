# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
    Builds the cabinet executable and stages a self-contained folder for
    Syncthing.

.DESCRIPTION
    The staged folder holds one executable, the ROM, and the licence texts.
    Nothing else is required at run time: the `windows-cabinet` preset links
    SDL3, Beetle PCE, and the Microsoft C runtime statically, so
    the cabinet needs no redistributable installed.

    Runtime state -- display assignment, backup RAM, quick states -- is
    written to `portable-data/` beside the executable and is deliberately
    excluded from the sync by the generated `.stignore`. Each machine keeps
    its own display layout and high scores.
#>

[CmdletBinding()]
param(
    # Where to stage. Defaults to a sibling of the workspace so that
    # Syncthing never sees the workspace itself.
    [string] $Destination,

    # The ROM to publish alongside the executable.
    [string] $RomPath,

    # Stage an executable that is already built rather than building first.
    [switch] $SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot

if (-not $Destination) {
    $Destination = Join-Path (Split-Path -Parent $repositoryRoot) "devils-crush-cabinet"
}
if (-not $RomPath) {
    $RomPath = Join-Path $repositoryRoot "Devil's Crush.pce"
}

if (-not (Test-Path -LiteralPath $RomPath)) {
    throw "ROM not found: $RomPath"
}

# Staging inside the workspace would defeat the point: Syncthing would carry
# the 18 GB of build output with it. Compare with a trailing separator so a
# sibling such as `devils-crush-cabinet` is not mistaken for a child.
$separator = [System.IO.Path]::DirectorySeparatorChar
$destinationFull = [System.IO.Path]::GetFullPath($Destination).TrimEnd($separator)
$workspaceFull = [System.IO.Path]::GetFullPath($repositoryRoot).TrimEnd($separator)
if ($destinationFull -eq $workspaceFull -or
    $destinationFull.StartsWith(
        $workspaceFull + $separator,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Destination must live outside the workspace: $destinationFull"
}

$buildRoot = Join-Path $repositoryRoot "out/build/windows-cabinet"

if (-not $SkipBuild) {
    Write-Host "Configuring $buildRoot" -ForegroundColor Cyan
    & cmake --preset windows-cabinet
    if ($LASTEXITCODE -ne 0) { throw "Configure failed with $LASTEXITCODE." }

    Write-Host "Building" -ForegroundColor Cyan
    & cmake --build --preset windows-cabinet
    if ($LASTEXITCODE -ne 0) { throw "Build failed with $LASTEXITCODE." }
}

Write-Host "Installing to $destinationFull" -ForegroundColor Cyan
& cmake --install $buildRoot --config Release --prefix $destinationFull --component Runtime
if ($LASTEXITCODE -ne 0) { throw "Install failed with $LASTEXITCODE." }

$executable = Join-Path $destinationFull "devils-crush.exe"
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Install did not produce $executable."
}

# The ROM is proprietary and therefore never part of the CMake install set.
$romName = Split-Path -Leaf $RomPath
Copy-Item -LiteralPath $RomPath -Destination (Join-Path $destinationFull $romName) -Force

# Syncthing reads this at the folder root. Runtime state is per-machine: the
# cabinet's display assignment must not travel back to the workstation, and
# the workstation's must not overwrite the cabinet's on the next push.
$stignore = @"
// Runtime state belongs to the machine that wrote it, not to the sync.
// Display assignment, backup RAM, and quick states all live here.
/portable-data
/logs
/captures
"@
$stignorePath = Join-Path $destinationFull ".stignore"

# A dot-prefixed file tends to end up Hidden on Windows, and .NET refuses to
# truncate a hidden file: it throws UnauthorizedAccessException rather than
# overwriting. Clear the attributes before writing and restore Hidden after,
# so restaging works and the file stays out of the way.
if (Test-Path -LiteralPath $stignorePath) {
    (Get-Item -LiteralPath $stignorePath -Force).Attributes = `
        [System.IO.FileAttributes]::Normal
}

# Windows PowerShell's `-Encoding utf8` emits a BOM, which Syncthing would
# read as part of the first pattern rather than as a comment marker.
[System.IO.File]::WriteAllText(
    $stignorePath,
    $stignore + [System.Environment]::NewLine,
    (New-Object System.Text.UTF8Encoding $false))

(Get-Item -LiteralPath $stignorePath -Force).Attributes = `
    [System.IO.FileAttributes]::Hidden

$size = (Get-Item -LiteralPath $executable).Length
$staged = Get-ChildItem -LiteralPath $destinationFull -File -Force |
    Sort-Object Name |
    ForEach-Object { "  {0,-28} {1,10:N0} bytes" -f $_.Name, $_.Length }

Write-Host ""
Write-Host "Staged $destinationFull" -ForegroundColor Green
$staged | Write-Host
Write-Host ""
Write-Host ("Executable is {0:N1} MiB and depends only on Windows system DLLs." -f ($size / 1MB))
Write-Host "Point Syncthing at the folder above. Run the game from inside it:"
Write-Host "  .\devils-crush.exe --cabinet"
