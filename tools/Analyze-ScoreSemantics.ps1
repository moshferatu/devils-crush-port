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

# F55C chooses the alternate accumulator only when $55 is nonzero. Its normal
# path selects a score slot through $2251, adds the unsigned 16-bit award in
# $30:$2F to the three-byte little-endian score, clamps it, updates the
# high-score relation, and marks score presentation dirty in $2CC0.
Assert-RomBytes $rom 0x00155C @(
    0xAD, 0xC7, 0x2C, 0xD0, 0x47,
    0xA5, 0x55, 0xF0, 0x19,
    0x18, 0xA5, 0x2F, 0x6D, 0xB7, 0x2D, 0x8D, 0xB7, 0x2D,
    0xA5, 0x30, 0x6D, 0xB8, 0x2D, 0x8D, 0xB8, 0x2D,
    0x62, 0x6D, 0xB9, 0x2D, 0x8D, 0xB9, 0x2D, 0x60
) "F55C accumulator selection"
Assert-RomBytes $rom 0x00157E @(
    0xAC, 0x51, 0x22, 0x18,
    0xA5, 0x2F, 0x79, 0x05, 0x22, 0x99, 0x05, 0x22,
    0xA5, 0x30, 0x79, 0x0C, 0x22, 0x99, 0x0C, 0x22,
    0xB9, 0x13, 0x22, 0x69, 0x00, 0x99, 0x13, 0x22,
    0x20, 0xA9, 0xF5, 0x20, 0xD6, 0xF5,
    0xAD, 0xC0, 0x2C, 0x09, 0x01, 0x8D, 0xC0, 0x2C, 0x60
) "F55C selected score accumulation"

# F5A9 compares the new little-endian value with 0x98967F (9,999,999) and
# saturates all three score bytes at that value when it is exceeded.
Assert-RomBytes $rom 0x0015A9 @(
    0xAC, 0x51, 0x22, 0x38,
    0xB9, 0x05, 0x22, 0xE9, 0x7F,
    0xB9, 0x0C, 0x22, 0xE9, 0x96,
    0xB9, 0x13, 0x22, 0xE9, 0x98,
    0x90, 0x17,
    0xA9, 0x7F, 0x99, 0x05, 0x22,
    0xA9, 0x96, 0x99, 0x0C, 0x22,
    0xA9, 0x98, 0x99, 0x13, 0x22,
    0xAD, 0xC0, 0x2C, 0x09, 0x80, 0x8D, 0xC0, 0x2C, 0x60
) "F5A9 score saturation"

$calls = Import-TraceCsv $CallPath
if ($calls.Count -ne 40) {
    throw "Expected 40 score calls, found $($calls.Count)."
}
foreach ($call in $calls) {
    if (
        $call.physical_bank -ne "00" -or
        $call.logical_pc -ne "F55C" -or
        $call.caller_pc -ne "F554" -or
        $call.return_pc -ne "F557" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "F5A8"
    ) {
        throw "Score call $($call.sequence) changed its bounded path."
    }
}

$writes = Import-TraceCsv $WritePath
$directStorePcs = @("F587", "F58F", "F597", "F5A5")
$expectedOffsets = @("0205", "020C", "0213", "0CC0")
$previousScore = 0
$awards = [System.Collections.Generic.HashSet[int]]::new()
foreach ($call in $calls) {
    $directWrites = @(
        $writes |
            Where-Object {
                $_.call_sequence -eq $call.sequence -and
                $_.instruction_physical_bank -eq "00" -and
                $directStorePcs -contains $_.instruction_logical_pc
            }
    )
    if ($directWrites.Count -ne 4) {
        throw (
            "Score call $($call.sequence) has $($directWrites.Count) " +
            "direct writes instead of four."
        )
    }
    if (
        ($directWrites.instruction_logical_pc -join ",") -ne
            ($directStorePcs -join ",") -or
        ($directWrites.work_ram_offset -join ",") -ne
            ($expectedOffsets -join ",")
    ) {
        throw "Score call $($call.sequence) changed direct write order."
    }

    $low = [Convert]::ToInt32($directWrites[0].value, 16)
    $middle = [Convert]::ToInt32($directWrites[1].value, 16)
    $high = [Convert]::ToInt32($directWrites[2].value, 16)
    $score = $low -bor ($middle -shl 8) -bor ($high -shl 16)
    $award = $score - $previousScore
    $entryAward = [Convert]::ToInt32($call.input_a, 16)
    if ($award -ne $entryAward) {
        throw (
            "Score call $($call.sequence) added $award, but entry A was " +
            "$entryAward."
        )
    }
    if ($directWrites[3].value -ne "01") {
        throw "Score call $($call.sequence) did not set dirty bit 0."
    }
    [void] $awards.Add($award)
    $previousScore = $score
}

if ($previousScore -ne 1280) {
    throw "Expected final stored score 1,280, found $previousScore."
}
$expectedAwards = @(10, 30, 40, 50, 100, 200)
if (
    (@($awards | Sort-Object) -join ",") -ne
    ($expectedAwards -join ",")
) {
    throw "Observed score-award set changed."
}

Write-Output "Score semantics verified."
Write-Output (
    "F55C: 40 slot-0 calls add unsigned 16-bit awards to the " +
    "little-endian 24-bit score at 2205/220C/2213."
)
Write-Output (
    "Observed awards: 10,30,40,50,100,200; final stored score: 1280."
)
Write-Output (
    "F5A9 saturates the stored score at 0x98967F (9999999)."
)
