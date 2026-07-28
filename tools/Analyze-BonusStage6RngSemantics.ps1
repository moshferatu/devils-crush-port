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

function Assert-RomHex {
    param(
        [byte[]] $Rom,
        [int] $Offset,
        [string] $Hex,
        [string] $Label
    )

    $expected = @(
        $Hex -split "\s+" |
            Where-Object { $_ -ne "" } |
            ForEach-Object { [Convert]::ToByte($_, 16) }
    )
    if ($Offset -lt 0 -or $Offset + $expected.Count -gt $Rom.Count) {
        throw "$Label is outside the ROM."
    }
    for ($index = 0; $index -lt $expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}: expected {1:X2}, " +
                "found {2:X2}."
            ) -f (
                $Offset + $index
            ), $expected[$index], $Rom[$Offset + $index]
        }
    }
}

function Import-TraceCsv {
    param([string] $Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Trace does not exist: $Path"
    }
    return @(
        Get-Content -LiteralPath $Path |
            Where-Object { -not $_.StartsWith("#") } |
            ConvertFrom-Csv
    )
}

function Assert-ScopedWrite {
    param(
        [object[]] $Writes,
        [int] $Call,
        [string] $Bank,
        [string] $Pc,
        [string] $Offset,
        [string] $Value,
        [string] $Label
    )

    $selected = @(
        $Writes |
            Where-Object {
                [int] $_.call_sequence -eq $Call -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($selected.Count -ne 1) {
        throw "$Label write changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Objective animation state 5 calls this allocator. It creates a type-C2
# hazard, derives a timer from one RNG result, derives a motion-table index
# from a second result, and emits command 32.
Assert-RomHex $rom 0x00B2E0 @"
20 24 C0 90 01 60 A9 C2 99 00 30 18 BD 80 31 69 28
99 80 31 62 7D 00 32 99 00 32 18 BD 00 33 69 08 99
00 33 62 7D 80 33 99 80 33 A9 3B 99 00 39 A9 84 99
80 30 A9 13 99 00 38 A9 81 99 00 3D A9 08 99 80 38
A9 4E 99 00 3F A9 73 99 80 3F 20 81 C0 29 1F 18 69
18 99 00 3B DA 02 20 81 C0 29 0F A8 B9 B1 73 20 72
C0 FA A9 32 20 87 C0 60
"@ "BS-6 hazard allocator and RNG consumers"

# The low nibble of the second result indexes this complete 16-byte motion
# selector table. Routed result C5 selects index 5 and value 5.
Assert-RomHex $rom 0x00B3B1 @"
02 02 02 03 04 05 06 06 06 06 06 05 04 03 02 02
"@ "BS-6 hazard motion selector table"

# The allocated hazard integrates until its timer expires, then enters its
# presentation/contact lifecycle.
Assert-RomHex $rom 0x00B34E @"
20 12 C0 90 03 4C 27 C0 DE 00 3B F0 01 60 A9 02 9D
80 39 A9 84 9D 00 3B A9 03 9D 80 3B
"@ "BS-6 emitted-hazard update"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
if ($calls.Count -ne 6 -or $writes.Count -ne 321) {
    throw "BS-6 RNG artifact populations changed."
}

$expectedCalls = @(
    @("1342", "00", "E8B3", "7217", "721A", "E8E6",
        "72", "34", "7E", "EC", "34", "7E", 282, 8),
    @("1342", "00", "E8B3", "7217", "721A", "E8E6",
        "72", "35", "00", "DF", "35", "00", 282, 8),
    @("1510", "00", "E8B3", "732E", "7331", "E8E6",
        "73", "35", "34", "A0", "35", "34", 282, 8),
    @("1510", "00", "E8B3", "733B", "733E", "E8E6",
        "18", "34", "35", "C5", "34", "35", 282, 8),
    @("1510", "05", "72E0", "72A4", "72A7", "734D",
        "05", "35", "7E", "04", "35", "14", 12924, 281),
    @("1534", "00", "E8B3", "7217", "721A", "E8E6",
        "09", "35", "00", "32", "35", "00", 282, 8)
)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $expected = $expectedCalls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.frame -ne $expected[0] -or
        $call.physical_bank -ne $expected[1] -or
        $call.logical_pc -ne $expected[2] -or
        $call.caller_pc -ne $expected[3] -or
        $call.return_pc -ne $expected[4] -or
        $call.return_instruction_pc -ne $expected[5] -or
        $call.input_a -ne $expected[6] -or
        $call.input_x -ne $expected[7] -or
        $call.input_y -ne $expected[8] -or
        $call.output_a -ne $expected[9] -or
        $call.output_x -ne $expected[10] -or
        $call.output_y -ne $expected[11] -or
        [int64] $call.body_master_cycles -ne $expected[12] -or
        [int] $call.write_count -ne $expected[13]
    ) {
        throw "BS-6 RNG call $($index + 1) changed."
    }
}

# Verify the exact recurrence of the five shared RNG calls. Calls 3 and 4
# are the newly activated allocator consumers; the others retain the stage's
# existing objective-initializer coverage.
$directPairs = @(
    "E8B9:0042", "E8C0:0043", "E8C2:0042", "E8C4:0043",
    "E8CC:0042", "E8D3:0043", "E8DB:022B", "E8E0:022A"
)
$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
foreach ($call in $rngCalls) {
    $directWrites = @(
        $writes |
            Where-Object {
                [int] $_.call_sequence -eq [int] $call.sequence -and
                $directPairs -contains (
                    "$($_.instruction_logical_pc):$($_.work_ram_offset)"
                )
            }
    )
    $actualPairs = @(
        $directWrites |
            ForEach-Object {
                "$($_.instruction_logical_pc):$($_.work_ram_offset)"
            }
    )
    if (
        $directWrites.Count -ne 8 -or
        ($actualPairs -join ",") -ne ($directPairs -join ",")
    ) {
        throw "BS-6 RNG call $($call.sequence) changed write order."
    }

    $values = @(
        $directWrites |
            ForEach-Object { [Convert]::ToInt32($_.value, 16) }
    )
    $oldLow = ($values[0] - 0x3F) -band 0xFF
    $constantCarry = [int] ($oldLow + 0x3F -gt 0xFF)
    $oldHigh = ($values[1] - 0x50 - $constantCarry) -band 0xFF
    $frameCounter = ([int] $call.frame - 1) -band 0xFF
    $frameCarry = [int] ($oldLow + $frameCounter -gt 0xFF)
    $expected = @(
        $values[0],
        $values[1],
        (($values[0] -shl 1) -band 0xFF),
        ((($values[1] -shl 1) + ($values[0] -shr 7)) -band 0xFF),
        (($oldLow + $frameCounter) -band 0xFF),
        0,
        0,
        0
    )
    $expected[5] = ($oldHigh + $expected[3] + $frameCarry) -band 0xFF
    $expected[6] = ($oldLow + $expected[5]) -band 0xFF
    $expected[7] = $expected[4]
    for ($step = 2; $step -lt $values.Count; ++$step) {
        if ($values[$step] -ne $expected[$step]) {
            throw (
                "BS-6 RNG recurrence changed in call $($call.sequence), " +
                "step $step."
            )
        }
    }
    if ([Convert]::ToInt32($call.output_a, 16) -ne $values[6]) {
        throw "BS-6 RNG call $($call.sequence) returned the wrong byte."
    }
}

# The natural terminal hit frees slot 34. Objective slot 35 later reaches
# animation state 5 and allocates the hazard back into that free slot.
Assert-ScopedWrite $writes 5 "05" "72E8" "1034" "C2" `
    "Hazard actor type"
Assert-ScopedWrite $writes 5 "05" "72F1" "11B4" "F0" `
    "Hazard horizontal position"
Assert-ScopedWrite $writes 5 "05" "7301" "1334" "40" `
    "Hazard vertical position"
Assert-ScopedWrite $writes 5 "05" "730D" "1934" "3B" `
    "Hazard collision shape"
Assert-ScopedWrite $writes 5 "05" "7312" "10B4" "84" `
    "Hazard actor flags"
Assert-ScopedWrite $writes 5 "05" "7317" "1834" "13" `
    "Hazard collision selector"
Assert-ScopedWrite $writes 5 "05" "731C" "1D34" "81" `
    "Hazard completion selector"
Assert-ScopedWrite $writes 5 "05" "7321" "18B4" "08" `
    "Hazard presentation state"
Assert-ScopedWrite $writes 5 "05" "7326" "1F34" "4E" `
    "Hazard dispatch low byte"
Assert-ScopedWrite $writes 5 "05" "732B" "1FB4" "73" `
    "Hazard dispatch high byte"

# A0 & 1F is zero, so the first RNG consumer stores the minimum timer 18.
Assert-ScopedWrite $writes 5 "05" "7336" "1B34" "18" `
    "Hazard RNG timer"

# C5 & 0F selects table index 5 and value 5. C072 installs the associated
# signed motion vector in the new hazard's slot.
Assert-ScopedWrite $writes 5 "01" "DBBE" "1434" "E8" `
    "Hazard horizontal velocity low byte"
Assert-ScopedWrite $writes 5 "01" "DBC4" "14B4" "FF" `
    "Hazard horizontal velocity high byte"
Assert-ScopedWrite $writes 5 "01" "DBCA" "1534" "3B" `
    "Hazard vertical velocity low byte"
Assert-ScopedWrite $writes 5 "01" "DBD0" "15B4" "00" `
    "Hazard vertical velocity high byte"
Assert-ScopedWrite $writes 5 "00" "ECFD" "002A" "32" `
    "Hazard allocation audio command"

Write-Output "BS-6 hazard RNG semantics verified."
Write-Output (
    "Consumer 732E: A0 & 1F plus 18 installs timer 18 in hazard slot 34."
)
Write-Output (
    "Consumer 733B: C5 selects motion-table index/value 5 and its signed " +
    "velocity vector."
)
Write-Output (
    "Allocator 72E0: objective slot 35 creates one type-C2 hazard in " +
    "12,924 master cycles with 281 ordered writes."
)
Write-Output "E8B3: all five routed calls preserve the eight-write recurrence."
