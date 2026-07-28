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

function Assert-ScopedWrites {
    param(
        [object[]] $Writes,
        [int] $Call,
        [string] $Bank,
        [string] $Pc,
        [int] $Count,
        [string] $Value,
        [string] $Label
    )

    $selected = @(
        $Writes |
            Where-Object {
                [int] $_.call_sequence -eq $Call -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.value -eq $Value
            }
    )
    if ($selected.Count -ne $Count) {
        throw "$Label writes changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Score saturation at 9,999,999 stored units sets bit 7 in session byte 2CC0.
# The main session dispatcher takes C239 whenever that byte is negative.
Assert-RomHex $rom 0x0015A9 @"
AC 51 22 38 B9 05 22 E9 7F B9 0C 22 E9 96 B9 13 22
E9 98 90 17 A9 7F 99 05 22 A9 96 99 0C 22 A9 98 99
13 22 AD C0 2C 09 80 8D C0 2C 60
"@ "Maximum-score saturation and ending request"
Assert-RomHex $rom 0x002190 @"
AD C0 2C 10 03 4C 39 C2
"@ "Maximum-score session dispatch"

# The bank-07 ending controller uses fixed slot 5 and an eight-frame cadence.
# On expiry, the first RNG low nibble selects effect geometry through 48DF.
# The second result's low bit selects audio command 2C or 2D.
Assert-RomHex $rom 0x00E899 @"
A2 05 A9 80 9D 00 30 A9 E0 9D 80 30 A9 01 9D 00 3A
A9 20 9D 80 31 20 18 C0 DE 00 3A F0 01 60 A9 08 9D
00 3A A5 A9 9D 00 33 A5 AA 9D 80 33 20 81 C0 29 0F
20 DF 48 20 81 C0 A0 2C 29 01 F0 02 A0 2D 98 20 87
C0 60
"@ "Maximum-score ending RNG controller"

# 48DF scales the low nibble into the 16-entry base-position table and then
# applies six signed offsets through allocator 4980.
Assert-RomHex $rom 0x00E8DF @"
0A 0A A8 B9 40 49 85 35 B9 41 49 85 36 B9 42 49 85
37 B9 43 49 85 38 C2 5A 98 0A 0A A8 18 A5 35 79 28
49 85 2B A5 36 79 29 49 85 2C 18 A5 37 79 2A 49 85
2D A5 38 79 2B 49 85 2E 20 80 49 B0 07 7A C8 C0 06
D0 D2 60 7A 60
"@ "Maximum-score six-effect geometry helper"
Assert-RomHex $rom 0x00E928 @"
00 00 00 00 E8 FF F0 FF 00 00 E0 FF E8 FF 08 00
18 00 F8 FF 10 00 10 00
"@ "Maximum-score six-effect offsets"
Assert-RomHex $rom 0x00E940 @"
20 00 20 00 40 00 20 00 80 00 20 00 C0 00 20 00
20 00 40 00 40 00 40 00 80 00 40 00 C0 00 40 00
20 00 80 00 40 00 80 00 80 00 80 00 C0 00 80 00
20 00 C0 00 40 00 C0 00 80 00 C0 00 C0 00 C0 00
"@ "Maximum-score 16-entry geometry table"
Assert-RomHex $rom 0x00E980 @"
20 24 C0 B0 53 18 BD 80 31 65 2B 99 80 31 BD 00 32
65 2C 99 00 32 18 BD 00 33 65 2D 99 00 33 BD 80 33
65 2E 99 80 33 62 99 00 38 99 80 39 B9 00 30 29 F0
09 0F 99 00 30 A9 DA 99 00 3F A9 49 99 80 3F A9 04
99 00 3A A9 10 99 00 39 BD 80 30 29 80 09 64 99 80
30 18 60 38 60
"@ "Maximum-score effect allocator"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
if ($calls.Count -ne 269 -or $writes.Count -ne 10041) {
    throw "Maximum-score RNG artifact populations changed."
}

$dispatchCalls = @($calls | Where-Object { $_.logical_pc -eq "48B2" })
$helperCalls = @($calls | Where-Object { $_.logical_pc -eq "48DF" })
$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
$firstConsumers = @($rngCalls | Where-Object { $_.caller_pc -eq "48C7" })
$secondConsumers = @($rngCalls | Where-Object { $_.caller_pc -eq "48CF" })
$eventCalls = @($dispatchCalls | Where-Object { [int] $_.write_count -eq 272 })
$idleCalls = @($dispatchCalls | Where-Object { [int] $_.write_count -eq 1 })
if (
    $dispatchCalls.Count -ne 184 -or
    $helperCalls.Count -ne 23 -or
    $rngCalls.Count -ne 62 -or
    $firstConsumers.Count -ne 23 -or
    $secondConsumers.Count -ne 23 -or
    $eventCalls.Count -ne 23 -or
    $idleCalls.Count -ne 161
) {
    throw "Maximum-score RNG call populations changed."
}

foreach ($call in $idleCalls) {
    if (
        $call.physical_bank -ne "07" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.return_physical_bank -ne "07" -or
        $call.return_instruction_pc -ne "48B7" -or
        [int64] $call.body_master_cycles -ne 27
    ) {
        throw "Maximum-score idle controller call changed."
    }
}

$expectedEventFrames = @(
    1618, 1625, 1633, 1641, 1649, 1657, 1665, 1673,
    1681, 1689, 1697, 1705, 1713, 1721, 1729, 1737,
    1745, 1753, 1761, 1769, 1777, 1785, 1793
)
$expectedEventCycles = @(
    11295, 12915, 14601, 16221, 17841, 11361, 12981, 14601,
    16155, 17841, 11361, 12915, 14535, 16221, 17775, 11361,
    12915, 14535, 16221, 16626, 12576, 12981, 14601
)
$expectedCommands = @(
    "2C", "2D", "2D", "2D", "2D", "2D", "2D", "2D",
    "2C", "2C", "2C", "2D", "2C", "2C", "2D", "2D",
    "2C", "2D", "2D", "2D", "2D", "2D", "2D"
)
for ($index = 0; $index -lt $eventCalls.Count; ++$index) {
    $call = $eventCalls[$index]
    if (
        [int] $call.frame -ne $expectedEventFrames[$index] -or
        $call.physical_bank -ne "07" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.return_physical_bank -ne "07" -or
        $call.return_instruction_pc -ne "48DE" -or
        $call.output_y -ne $expectedCommands[$index] -or
        [int64] $call.body_master_cycles -ne $expectedEventCycles[$index]
    ) {
        throw "Maximum-score event controller call $($index + 1) changed."
    }
}

$expectedFirstOutputs = @(
    "56", "F8", "6D", "9B", "80", "12", "FC", "3E",
    "D8", "CE", "3D", "24", "8F", "E1", "83", "42",
    "3D", "94", "70", "34", "5F", "67", "76"
)
$expectedSecondOutputs = @(
    "28", "BD", "DF", "39", "C7", "6D", "2B", "01",
    "F0", "02", "8C", "93", "36", "9A", "01", "CD",
    "5C", "13", "67", "83", "E3", "EB", "17"
)
$expectedHelperInputs = @(
    "06", "08", "0D", "0B", "00", "02", "0C", "0E",
    "08", "0E", "0D", "04", "0F", "01", "03", "02",
    "0D", "04", "00", "04", "0F", "07", "06"
)
for ($index = 0; $index -lt 23; ++$index) {
    $first = $firstConsumers[$index]
    $second = $secondConsumers[$index]
    $helper = $helperCalls[$index]
    $derivedCommand = if (
        (
            [Convert]::ToInt32($second.output_a, 16) -band 1
        ) -eq 0
    ) {
        "2C"
    } else {
        "2D"
    }
    if (
        [int] $first.frame -ne $expectedEventFrames[$index] -or
        [int] $second.frame -ne $expectedEventFrames[$index] -or
        [int] $helper.frame -ne $expectedEventFrames[$index] -or
        $first.output_a -ne $expectedFirstOutputs[$index] -or
        $second.output_a -ne $expectedSecondOutputs[$index] -or
        $helper.input_a -ne $expectedHelperInputs[$index] -or
        $helper.input_a -ne (
            "{0:X2}" -f (
                [Convert]::ToInt32($first.output_a, 16) -band 0x0F
            )
        ) -or
        $expectedCommands[$index] -ne $derivedCommand -or
        $helper.physical_bank -ne "07" -or
        $helper.caller_pc -ne "48CC" -or
        $helper.return_pc -ne "48CF" -or
        $helper.return_instruction_pc -ne "4925" -or
        [int] $helper.write_count -ne 136
    ) {
        throw "Maximum-score RNG event $($index + 1) changed."
    }
}

$rngCallerCounts = @{}
foreach ($call in $rngCalls) {
    if (-not $rngCallerCounts.ContainsKey($call.caller_pc)) {
        $rngCallerCounts[$call.caller_pc] = 0
    }
    ++$rngCallerCounts[$call.caller_pc]
}
$expectedRngCallerCounts = @{
    "6678" = 3
    "6697" = 4
    "68B1" = 7
    "597D" = 2
    "48C7" = 23
    "48CF" = 23
}
if ($rngCallerCounts.Count -ne $expectedRngCallerCounts.Count) {
    throw "Maximum-score RNG caller set changed."
}
foreach ($caller in $expectedRngCallerCounts.Keys) {
    if (
        -not $rngCallerCounts.ContainsKey($caller) -or
        $rngCallerCounts[$caller] -ne $expectedRngCallerCounts[$caller]
    ) {
        throw "Maximum-score RNG caller count changed for $caller."
    }
}

# Reconstruct every common RNG call, including the normal-table calls before
# the ending transition.
$directPairs = @(
    "E8B9:0042", "E8C0:0043", "E8C2:0042", "E8C4:0043",
    "E8CC:0042", "E8D3:0043", "E8DB:022B", "E8E0:022A"
)
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
        throw "Maximum-score RNG call $($call.sequence) changed write order."
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
                "Maximum-score RNG recurrence changed in call " +
                "$($call.sequence), step $step."
            )
        }
    }
    if ([Convert]::ToInt32($call.output_a, 16) -ne $values[6]) {
        throw "Maximum-score RNG call $($call.sequence) returned the wrong byte."
    }
}

# Each geometry helper creates six type-8F actors with dispatch 49DA and the
# common presentation fields. The outer event scope must also retain the
# selected audio command.
for ($index = 0; $index -lt $helperCalls.Count; ++$index) {
    $helper = $helperCalls[$index]
    Assert-ScopedWrites $writes $helper.sequence "07" "49B5" 6 "8F" `
        "Ending effect type"
    Assert-ScopedWrites $writes $helper.sequence "07" "49BA" 6 "DA" `
        "Ending effect dispatch low byte"
    Assert-ScopedWrites $writes $helper.sequence "07" "49BF" 6 "49" `
        "Ending effect dispatch high byte"
    Assert-ScopedWrites $writes $helper.sequence "07" "49C4" 6 "04" `
        "Ending effect lifetime"
    Assert-ScopedWrites $writes $helper.sequence "07" "49C9" 6 "10" `
        "Ending effect collision field"

    $event = $eventCalls[$index]
    $audioWrites = @(
        $writes |
            Where-Object {
                [int] $_.call_sequence -eq [int] $event.sequence -and
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq "ECFD" -and
                $_.work_ram_offset -eq "002A" -and
                $_.value -eq $expectedCommands[$index]
            }
    )
    if ($audioWrites.Count -ne 1) {
        throw "Maximum-score event audio command $($index + 1) changed."
    }
}

Write-Output "Maximum-score ending RNG semantics verified."
Write-Output (
    "Transition: score saturation sets 2CC0.7 and the session dispatcher " +
    "enters the bank-07 ending."
)
Write-Output (
    "Consumer 48C7: 23 low nibbles select 15 of 16 six-effect geometries."
)
Write-Output (
    "Consumer 48CF: 23 low bits select command 2C seven times and 2D " +
    "sixteen times."
)
Write-Output (
    "Ending effect: every event allocates six type-8F actors; all 62 routed " +
    "RNG calls preserve the eight-write recurrence."
)
