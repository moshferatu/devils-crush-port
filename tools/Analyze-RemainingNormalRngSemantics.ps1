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

# The Devil Shooter's reset state clears its ball/session fields. In the
# automatic-input mode selected by $2254 it consumes RNG, maps the high byte
# into $40-$7F, and submits that selector through common state helper C021.
Assert-RomHex $rom 0x005124 @"
BD 00 38 29 7F 9D 00 38 64 BD A5 53 29 FD 85 53
9E 00 35 9E 80 35 9E 80 39 9E 80 3C 9C 15 2D AD
54 22 F0 0E 20 81 C0 29 3F 18 69 40 20 21 C0 20
79 51 20 18 C0
"@ "Devil-Shooter automatic reset and RNG consumer"

# The Undead Head launch state releases the ball, selects vector $17/$19
# from one random bit, integrates it, and restores the actor presentation.
Assert-RomHex $rom 0x006165 @"
DA 82 9C 80 3C AD 80 30 29 DF 8D 80 30 A9 08 9D
80 38 20 81 C0 A0 17 29 01 F0 02 A0 19 98 20 75
C0 20 1E C0 FA A9 08 20 21 C0 20 18 C0
"@ "Undead-Head launch RNG consumer"

# The Giant Face mouth-release continuation closes the mouth, switches to
# ball slot zero, selects vector $07/$09, launches the ball, and restores
# fixed mouth actor $38.
Assert-RomHex $rom 0x007821 @"
9C 21 2D A9 06 20 21 C0 20 18 C0 02 A2 00 A9 0A
9D 80 38 20 81 C0 A0 07 29 01 F0 02 A0 09 98 20
75 C0 20 1E C0 AD 80 3C 29 FE 8D 80 3C A6 4D A9
82 9D 00 30 A9 04 20 21 C0 20 18 C0
"@ "Giant-Face mouth-release RNG consumer"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
if ($calls.Count -ne 28 -or $writes.Count -ne 1195) {
    throw "Remaining normal-table RNG artifact populations changed."
}

$expectedRoutineCounts = @{
    "5124" = 21
    "5148" = 1
    "6165" = 1
    "7821" = 1
    "7834" = 1
    "E8B3" = 3
}
$actualRoutineCounts = @{}
foreach ($group in @($calls | Group-Object -Property logical_pc)) {
    $actualRoutineCounts[$group.Name] = $group.Count
}
if (
    $actualRoutineCounts.Count -ne $expectedRoutineCounts.Count -or
    @(
        $expectedRoutineCounts.GetEnumerator() |
            Where-Object {
                -not $actualRoutineCounts.ContainsKey($_.Key) -or
                $actualRoutineCounts[$_.Key] -ne $_.Value
            }
    ).Count -ne 0
) {
    throw "Remaining normal-table RNG routine histogram changed."
}

$expectedLeadingCalls = @(
    @("1084", "00", "E8B3", "5148", "00", "E8E6",
        "01", "0E", "78", "E7", "0E", "78", 282, 8),
    @("1084", "02", "5124", "C51D", "01", "C5E9",
        "00", "0E", "78", "67", "0E", "20", 10374, 248),
    @("1084", "02", "5148", "C51D", "01", "C5E9",
        "01", "0E", "78", "67", "0E", "20", 10200, 240),
    @("1215", "03", "7821", "C51D", "01", "C5E9",
        "78", "38", "72", "06", "38", "72", 357, 14),
    @("1222", "00", "E8B3", "7834", "00", "E8E6",
        "0A", "00", "38", "58", "00", "38", 282, 8),
    @("1222", "03", "7834", "C51D", "01", "C5E9",
        "0A", "00", "38", "04", "38", "02", 5496, 167),
    @("1223", "00", "E8B3", "6177", "00", "E8E6",
        "08", "00", "03", "70", "00", "03", 975, 21),
    @("1223", "03", "6165", "C51D", "01", "C5E9",
        "61", "4D", "03", "08", "4D", "02", 5778, 177)
)
for ($index = 0; $index -lt $expectedLeadingCalls.Count; ++$index) {
    $call = $calls[$index]
    $expected = $expectedLeadingCalls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.frame -ne $expected[0] -or
        $call.physical_bank -ne $expected[1] -or
        $call.logical_pc -ne $expected[2] -or
        $call.caller_pc -ne $expected[3] -or
        $call.return_physical_bank -ne $expected[4] -or
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
        throw "Remaining normal-table RNG call $($index + 1) changed."
    }
}

# All three consumer calls preserve the common RNG routine's exact eight
# direct writes. The Undead-Head call includes a periodic interrupt, but the
# RNG-owned recurrence and returned high byte remain unchanged.
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
        throw "RNG call $($call.sequence) changed direct write order."
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
                "RNG recurrence changed in call $($call.sequence), " +
                "step $step."
            )
        }
    }
    if ([Convert]::ToInt32($call.output_a, 16) -ne $values[6]) {
        throw "RNG call $($call.sequence) returned the wrong byte."
    }
}

# E7 maps to $67 and C021 stores that automatic state selector for fixed
# Devil-Shooter slot $0E.
Assert-ScopedWrite $writes 3 "01" "C5EA" "1D8E" "67" `
    "Devil-Shooter randomized selector"

# Result $58 is even, so the Giant-Face release selects vector $07:
# signed velocity (+12,+62) for ball slot zero.
Assert-ScopedWrite $writes 6 "01" "DBBE" "1400" "0C" `
    "Giant-Face horizontal velocity low byte"
Assert-ScopedWrite $writes 6 "01" "DBC4" "1480" "00" `
    "Giant-Face horizontal velocity high byte"
Assert-ScopedWrite $writes 6 "01" "DBCA" "1500" "3E" `
    "Giant-Face vertical velocity low byte"
Assert-ScopedWrite $writes 6 "01" "DBD0" "1580" "00" `
    "Giant-Face vertical velocity high byte"
Assert-ScopedWrite $writes 6 "03" "784B" "1C80" "00" `
    "Giant-Face ball release flag"
Assert-ScopedWrite $writes 6 "03" "7852" "1038" "82" `
    "Giant-Face mouth actor restoration"

# Result $70 is even, so the Undead Head selects vector $17:
# signed velocity (-12,-62) for ball slot zero.
Assert-ScopedWrite $writes 8 "03" "6167" "1C80" "00" `
    "Undead-Head ball release flag"
Assert-ScopedWrite $writes 8 "03" "616F" "1080" "80" `
    "Undead-Head ball activation"
Assert-ScopedWrite $writes 8 "03" "6174" "1880" "08" `
    "Undead-Head ball response"
Assert-ScopedWrite $writes 8 "01" "DBBE" "1400" "F4" `
    "Undead-Head horizontal velocity low byte"
Assert-ScopedWrite $writes 8 "01" "DBC4" "1480" "FF" `
    "Undead-Head horizontal velocity high byte"
Assert-ScopedWrite $writes 8 "01" "DBCA" "1500" "C2" `
    "Undead-Head vertical velocity low byte"
Assert-ScopedWrite $writes 8 "01" "DBD0" "1580" "FF" `
    "Undead-Head vertical velocity high byte"

Write-Output "Remaining normal-table RNG semantics verified."
Write-Output (
    "Consumer 02:5148: result E7 maps to selector 67 for Devil-Shooter " +
    "slot 0E."
)
Write-Output (
    "Consumer 03:7834: even result 58 selects vector 07 and velocity " +
    "(+12,+62)."
)
Write-Output (
    "Consumer 03:6177: even result 70 selects vector 17 and velocity " +
    "(-12,-62)."
)
Write-Output (
    "E8B3: all three routed calls preserve the exact eight-write recurrence."
)
