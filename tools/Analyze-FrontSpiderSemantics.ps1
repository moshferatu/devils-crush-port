# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $CallPath,

    [Parameter(Mandatory = $true)]
    [string] $WritePath,

    [Parameter(Mandatory = $true)]
    [string] $ActorWritePath
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

function Assert-Histogram {
    param(
        [object[]] $Rows,
        [string] $Property,
        [hashtable] $Expected,
        [string] $Label
    )

    $actual = @{}
    foreach ($group in @($Rows | Group-Object -Property $Property)) {
        $actual[$group.Name] = $group.Count
    }
    if ($actual.Count -ne $Expected.Count) {
        throw "$Label changed population."
    }
    foreach ($key in $Expected.Keys) {
        if (
            -not $actual.ContainsKey($key) -or
            $actual[$key] -ne $Expected[$key]
        ) {
            throw (
                "$Label changed ${key}: expected $($Expected[$key]), " +
                "found $($actual[$key])."
            )
        }
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# These fixed type-$83 records sit directly below the two bumpers. Their
# shared initializer stores the record parameter as side index 0/1. Player
# documentation calls them Front Spiders; the USA manual groups the spider
# artwork under its Skeleton Insect terminology.
Assert-RomBytes $rom 0x00472B @(
    0x00, 0x26, 0x6B, 0x64, 0x40, 0x00,
    0x7C, 0x02, 0x29, 0x82, 0x83, 0x00,
    0x00, 0x27, 0x6B, 0x64, 0xD8, 0x00,
    0x7C, 0x02, 0x29, 0x82, 0x83, 0x01
) "Front-spider layout records"
Assert-RomBytes $rom 0x00646B @(
    0x9D, 0x80, 0x3C,
    0xA9, 0x10, 0x9D, 0x00, 0x38,
    0xA9, 0x8C, 0x9D, 0x00, 0x3D,
    0xA9, 0x80, 0x9D, 0x80, 0x38,
    0xA9, 0x01, 0x9D, 0x00, 0x3A,
    0x9E, 0x80, 0x39,
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x9D, 0x00, 0x38,
    0x20, 0x18, 0xC0
) "Front-spider initializer"

# The steady routine selects side-specific collision ID $2A/$2B. On a hit it
# sets $2D13,Y for the left trap-door or right launcher safety, emits command
# $34, awards 40 stored units, and runs the common appearance transition
# before returning to 6490.
Assert-RomBytes $rom 0x006490 @(
    0xA9, 0x2A, 0xBC, 0x80, 0x3C, 0xF0, 0x02, 0xA9, 0x2B,
    0x20, 0x66, 0x69, 0x20, 0xA4, 0x65,
    0xBD, 0x00, 0x38, 0x30, 0x01, 0x60,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xBC, 0x80, 0x3C,
    0xA9, 0x01, 0x99, 0x13, 0x2D,
    0xA9, 0x34, 0x20, 0x87, 0xC0,
    0xA9, 0x28, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x04, 0x9D, 0x80, 0x39,
    0xBD, 0x80, 0x30, 0x09, 0x40, 0x9D, 0x80, 0x30,
    0xA9, 0x0C, 0x20, 0x21, 0xC0, 0x20, 0x18, 0xC0,
    0xFE, 0x80, 0x39,
    0xA9, 0x0C, 0x20, 0x21, 0xC0, 0x20, 0x18, 0xC0,
    0xBD, 0x80, 0x30, 0x09, 0x20, 0x9D, 0x80, 0x30,
    0xA9, 0x30, 0x20, 0x21, 0xC0, 0x20, 0x18, 0xC0,
    0xBD, 0x80, 0x30, 0x29, 0x9F, 0x9D, 0x80, 0x30,
    0x80, 0x83
) "Front-spider steady hit and reset rule"
Assert-RomBytes $rom 0x00202A @(0x4C, 0x4F, 0xF5) "Score entry"
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 2645 -or
    $writes.Count -ne 39361 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 2,645 Front-Spider calls, 39,361 scoped writes, and " +
        "3,872 actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne "6490" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw (
            "Front-Spider bounded-call identity changed at sequence " +
            "$($index + 1)."
        )
    }
    # Appearance helpers intentionally remap MPR4/MPR5.
    foreach ($mpr in @(0, 1, 2, 3, 6, 7)) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Front Spider changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "input_x" @{
    "26" = 1357
    "27" = 1288
} "Front-Spider slot"
Assert-Histogram $calls "return_instruction_pc" @{
    "64A4" = 2642
    "C5E9" = 3
} "Front-Spider return"

$bodyCycles = @($calls | ForEach-Object { [int64] $_.body_master_cycles })
$writeCounts = @($calls | ForEach-Object { [int] $_.write_count })
if (
    [int] $calls[0].frame -ne 873 -or
    [int] $calls[-1].frame -ne 3551 -or
    ($bodyCycles | Measure-Object -Minimum).Minimum -ne 336 -or
    ($bodyCycles | Measure-Object -Maximum).Maximum -ne 6333 -or
    ($writeCounts | Measure-Object -Minimum).Minimum -ne 9 -or
    ($writeCounts | Measure-Object -Maximum).Maximum -ne 153 -or
    [int64] $calls[0].body_master_cycles -ne 903 -or
    [int] $calls[0].write_count -ne 19
) {
    throw "Front-Spider frame, timing, write, or replay sample changed."
}

$hitMarkers = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "64B9" -and
            $_.work_ram_offset -eq "002F" -and
            $_.value -eq "28"
        }
)
$expectedSequences = @(672, 1364, 2547)
$expectedFrames = @(2182, 2564, 3466)
$expectedSlots = @("27", "27", "26")
$expectedSafetyOffsets = @("0D14", "0D14", "0D13")
if ($hitMarkers.Count -ne 3) {
    throw "Expected three routed Front-Spider hits."
}

for ($index = 0; $index -lt $hitMarkers.Count; ++$index) {
    $sequence = [int] $hitMarkers[$index].call_sequence
    $call = $calls[$sequence - 1]
    $callWrites = @(
        $writes |
            Where-Object { [int] $_.call_sequence -eq $sequence }
    )
    $safetyWrites = @(
        $callWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq "64AF" -and
                $_.work_ram_offset -eq $expectedSafetyOffsets[$index] -and
                $_.value -eq "01"
            }
    )
    $audioWrites = @(
        $callWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq "ECFD" -and
                $_.work_ram_offset -eq "002A" -and
                $_.value -eq "34"
            }
    )
    $scoreHighWrites = @(
        $callWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq "64BB" -and
                $_.work_ram_offset -eq "0030" -and
                $_.value -eq "00"
            }
    )
    if (
        $sequence -ne $expectedSequences[$index] -or
        [int] $call.frame -ne $expectedFrames[$index] -or
        $call.input_x -ne $expectedSlots[$index] -or
        $call.return_physical_bank -ne "01" -or
        $call.return_instruction_pc -ne "C5E9" -or
        $safetyWrites.Count -ne 1 -or
        $audioWrites.Count -ne 1 -or
        $scoreHighWrites.Count -ne 1
    ) {
        throw "Front-Spider hit $($index + 1) changed."
    }
}

# Each hit begins the same four-stage appearance transition and returns to
# steady 6490. The two right-side hits and one left-side hit are all visible
# in the global dispatch trace.
$transitionStarts = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5FB" -and
            $_.logical_address -in @("3F26", "3F27") -and
            $_.value -eq "D2"
        }
)
$steadyReturns = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            $_.logical_address -in @("3F26", "3F27") -and
            $_.value -eq "90" -and
            [int] $_.frame -gt 872
        }
)
$fixedTypes = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4131" -and
            $_.logical_address -in @("3026", "3027") -and
            $_.value -eq "83"
        }
)
if (
    ($transitionStarts.frame -join ",") -ne "2182,2564,3466" -or
    ($transitionStarts.logical_address -join ",") -ne "3F27,3F27,3F26" -or
    ($steadyReturns.frame -join ",") -ne "2254,2636,3538" -or
    ($steadyReturns.logical_address -join ",") -ne "3F27,3F27,3F26" -or
    ($fixedTypes.logical_address -join ",") -ne "3026,3027"
) {
    throw "Front-Spider fixed identity or transition timeline changed."
}

Write-Output "Front-Spider semantics verified."
Write-Output (
    "Fixed Front Spiders: slots 26/27 at (64/216,636), dispatch " +
    "03:6490, 2645 calls."
)
Write-Output (
    "Hit rule: frames 2182/2564/3466 set side safety flags " +
    "2D14/2D14/2D13, award 40, and emit command 34."
)
Write-Output (
    "Observed score contribution: 120 stored units; both right-side hits " +
    "and the left-side hit return through the same appearance cycle."
)
Write-Output (
    "First normalized sample: 03:6490@1, frame 873, 903 cycles, " +
    "19 writes."
)
