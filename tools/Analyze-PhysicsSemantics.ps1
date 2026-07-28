# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $CallPath,

    [Parameter(Mandatory = $true)]
    [string] $WritePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-RomBytes {
    param(
        [byte[]] $Rom,
        [int] $Offset,
        [byte[]] $Expected,
        [string] $Label
    )

    if ($Offset -lt 0 -or $Offset + $Expected.Count -gt $Rom.Count) {
        throw "$Label is outside the ROM."
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $Expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}: expected {1:X2}, " +
                "found {2:X2}."
            ) -f (
                $Offset + $index
            ), $Expected[$index], $Rom[$Offset + $index]
        }
    }
}

function Import-TraceCsv {
    param(
        [string] $Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Trace does not exist: $Path"
    }
    return @(
        Get-Content -LiteralPath $Path |
            Where-Object { -not $_.StartsWith("#") } |
            ConvertFrom-Csv
    )
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# F1B4 caches two three-byte positions, adds two sign-extended 8.8
# increments, and tests whether either integer portion changed.
Assert-RomBytes $rom 0x0011B4 @(
    0xBD, 0x80, 0x32, 0x85, 0x88, 0xBD, 0x00, 0x33, 0x85, 0x89,
    0xBD, 0x80, 0x33, 0x85, 0x8A, 0xBD, 0x00, 0x31, 0x85, 0x85,
    0xBD, 0x80, 0x31, 0x85, 0x86, 0xBD, 0x00, 0x32, 0x85, 0x87
) "F1B4 position cache"
Assert-RomBytes $rom 0x0011D2 @(
    0xBD, 0x80, 0x3C, 0x29, 0xFD, 0x9D, 0x80, 0x3C, 0x64, 0x2B,
    0xB5, 0xBC, 0x10, 0x02, 0xC6, 0x2B, 0x18, 0xBD, 0x80, 0x32,
    0x75, 0xBB, 0x9D, 0x80, 0x32, 0xBD, 0x00, 0x33, 0x75, 0xBC
) "F1B4 first signed 8.8 integration chain"
Assert-RomBytes $rom 0x0011FB @(
    0x64, 0x2B, 0xB5, 0xBA, 0x10, 0x02, 0xC6, 0x2B, 0x18,
    0xBD, 0x00, 0x31, 0x75, 0xB9, 0x9D, 0x00, 0x31,
    0xBD, 0x80, 0x31, 0x75, 0xBA, 0x9D, 0x80, 0x31,
    0xBD, 0x00, 0x32, 0x65, 0x2B, 0x9D, 0x00, 0x32
) "F1B4 second signed 8.8 integration chain"

# F241 is an exact six-byte restore from the F1B4 cache.
Assert-RomBytes $rom 0x001241 @(
    0xA5, 0x85, 0x9D, 0x00, 0x31,
    0xA5, 0x86, 0x9D, 0x80, 0x31,
    0xA5, 0x87, 0x9D, 0x00, 0x32,
    0xA5, 0x88, 0x9D, 0x80, 0x32,
    0xA5, 0x89, 0x9D, 0x00, 0x33,
    0xA5, 0x8A, 0x9D, 0x80, 0x33,
    0x60
) "F241 cached-position restore"

# D1A2 uses the signed fixed-point multiplier at E65C and the signed negate
# helper at E837 to form a two-dimensional rotation matrix.
Assert-RomBytes $rom 0x0031A2 @(
    0xA5, 0xC0, 0x85, 0x2F, 0xA5, 0xC1, 0x85, 0x30,
    0xA5, 0x7D, 0x85, 0x2D, 0xA5, 0x7E, 0x85, 0x2E,
    0x20, 0x5C, 0xE6
) "D1A2 first signed fixed-point product"
Assert-RomBytes $rom 0x0031C2 @(
    0x2F, 0xA5, 0xBF, 0x85, 0x30, 0xA5, 0x7F, 0x85, 0x2D,
    0xA5, 0x80, 0x85, 0x2E, 0x20, 0x5C, 0xE6, 0x20, 0x37, 0xE8
) "D1A2 negated cross product"
Assert-RomBytes $rom 0x0031F2 @(
    0xA5, 0x80, 0x85, 0x2E, 0x20, 0x5C, 0xE6,
    0xA5, 0x2F, 0x9D, 0x00, 0x35,
    0xA5, 0x30, 0x9D, 0x80, 0x35
) "D1A2 second output component"
Assert-RomBytes $rom 0x000837 @(
    0xA5, 0x2F, 0x49, 0xFF, 0x85, 0x2F,
    0xA5, 0x30, 0x49, 0xFF, 0x85, 0x30,
    0xE6, 0x2F, 0xD0, 0x02, 0xE6, 0x30, 0x60
) "E837 signed negate helper"

# DD09 subtracts signed quotients produced by E905, then adds the unsigned
# low-byte acceleration at 8B to the second component.
Assert-RomBytes $rom 0x003D09 @(
    0xBD, 0x00, 0x34, 0x1D, 0x80, 0x34, 0xF0, 0x22,
    0xBD, 0x00, 0x34, 0x85, 0x2F, 0xBD, 0x80, 0x34, 0x85, 0x30,
    0xA5, 0x8C, 0x85, 0x2D, 0x20, 0x05, 0xE9
) "DD09 first signed damping division"
Assert-RomBytes $rom 0x003D5D @(
    0x18, 0xBD, 0x00, 0x35, 0x65, 0x8B, 0x9D, 0x00, 0x35,
    0x62, 0x7D, 0x80, 0x35, 0x9D, 0x80, 0x35, 0x60
) "DD09 second-axis acceleration"
Assert-RomBytes $rom 0x000905 @(
    0xA5, 0x2D, 0x85, 0x46, 0x10, 0x05, 0x49, 0xFF, 0x1A, 0x85, 0x2D,
    0xA5, 0x30, 0x45, 0x46, 0x85, 0x46, 0xA5, 0x30, 0x10, 0x0B,
    0x38, 0x62, 0xE5, 0x2F, 0x85, 0x2F, 0x62, 0xE5, 0x30, 0x85, 0x30,
    0x20, 0xE7, 0xE8
) "E905 signed division wrapper"

$calls = Import-TraceCsv $CallPath
if ($calls.Count -ne 2717) {
    throw "Expected 2,717 physics calls, found $($calls.Count)."
}
if (@($calls | Where-Object { $_.input_x -ne "00" }).Count -ne 0) {
    throw "The physics route entered a nonzero indexed slot."
}

$expectedCounts = @{
    "00:F1B4" = 1156
    "00:F241" = 404
    "01:D1A2" = 404
    "01:DD09" = 753
}
foreach ($key in $expectedCounts.Keys) {
    $parts = $key.Split(":")
    $count = @(
        $calls |
            Where-Object {
                $_.physical_bank -eq $parts[0] -and
                $_.logical_pc -eq $parts[1]
            }
    ).Count
    if ($count -ne $expectedCounts[$key]) {
        throw "$key call count changed: $count."
    }
}

$f241Calls = @(
    $calls |
        Where-Object {
            $_.physical_bank -eq "00" -and $_.logical_pc -eq "F241"
        }
)
$f241Sequences = @{}
foreach ($call in $f241Calls) {
    $f241Sequences[$call.sequence] = $true
}
$f241StorePcs = @{
    "F243" = $true
    "F248" = $true
    "F24D" = $true
    "F252" = $true
    "F257" = $true
    "F25C" = $true
}
$expectedOffsets = @("1100", "1180", "1200", "1280", "1300", "1380")
$expectedOffsetSet = @{}
foreach ($offset in $expectedOffsets) {
    $expectedOffsetSet[$offset] = $true
}
$f241Writes = @(
    Import-TraceCsv $WritePath |
        Where-Object {
            $f241Sequences.ContainsKey($_.call_sequence) -and
            $_.instruction_physical_bank -eq "00" -and
            $f241StorePcs.ContainsKey($_.instruction_logical_pc) -and
            $expectedOffsetSet.ContainsKey($_.work_ram_offset)
        }
)
if ($f241Writes.Count -ne 2424) {
    throw "Expected 2,424 F241 writes, found $($f241Writes.Count)."
}
foreach ($group in ($f241Writes | Group-Object call_sequence)) {
    $actualOffsets = @($group.Group | ForEach-Object { $_.work_ram_offset })
    if (($actualOffsets -join ",") -ne ($expectedOffsets -join ",")) {
        throw "F241 write order changed in call $($group.Name)."
    }
}

Write-Output "Physics semantics verified."
Write-Output "Indexed physics slot: X=00 across 2717 calls; Y is caller context."
Write-Output (
    "F1B4: two signed 16.8 positions integrate signed 8.8 increments; " +
    "bit 1 at 3C80 marks no integer-position change."
)
Write-Output "F241: 404 calls restore six cached pre-integration position bytes."
Write-Output (
    "D1A2: signed 8.8 matrix rotation produces the two 3400/3480 and " +
    "3500/3580 components."
)
Write-Output (
    "DD09: signed component damping subtracts value/divisor, then 8B " +
    "accelerates the second 8.8 component."
)
