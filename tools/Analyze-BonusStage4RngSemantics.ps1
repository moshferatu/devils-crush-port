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
    [string] $StatePath
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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Both linked-target states use a 32-tick timer. On expiry they call the
# shared RNG, mask its high-byte result to five bits, pass that value to the
# common motion helper, and then integrate the actor.
Assert-RomHex $rom 0x00AB7E @"
DE 00 3B D0 0D A9 20 9D 00 3B 20 81 C0 29 1F 20 75
C0 20 12 C0
"@ "BS-4 linked-target RNG state"
Assert-RomHex $rom 0x00ACA0 @"
7F 9D 00 38 20 18 C0 DE 00 3B D0 0D A9 20 9D 00 3B
20 81 C0 29 1F 20 75 C0 20 12 C0
"@ "BS-4 post-allocation RNG state"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 40 -or
    $writes.Count -ne 320 -or
    $states.Count -ne 16779
) {
    throw "BS-4 RNG artifact populations changed."
}

$expectedCalls = @(
    "1404:66:2B:6B88", "1405:67:1A:6CB1",
    "1436:66:81:6B88", "1437:67:50:6CB1",
    "1468:66:7B:6B88", "1469:67:B7:6CB1",
    "1500:66:46:6B88", "1501:67:CF:6CB1",
    "1533:67:67:6CB1", "1538:66:2F:6CB1",
    "1565:67:A3:6CB1", "1570:66:1F:6CB1",
    "1597:67:D2:6CB1", "1600:68:27:6B88",
    "1600:6D:66:6B88", "1602:66:64:6CB1",
    "1629:67:BC:6CB1", "1632:68:20:6B88",
    "1632:6D:AD:6B88", "1634:66:B2:6CB1",
    "1661:67:40:6CB1", "1664:68:67:6B88",
    "1664:6D:5D:6B88", "1666:66:BE:6CB1",
    "1693:67:7F:6CB1", "1696:68:5F:6B88",
    "1696:6D:A1:6B88", "1698:66:06:6CB1",
    "1725:67:EF:6CB1", "1728:68:6C:6B88",
    "1728:6D:A1:6B88", "1730:66:01:6CB1",
    "1757:67:FD:6CB1", "1760:68:CF:6B88",
    "1760:6D:23:6B88", "1762:66:03:6CB1",
    "1789:67:9F:6CB1", "1792:68:72:6B88",
    "1792:6D:EA:6B88", "1794:66:52:6CB1"
)
$pathCounts = @{}
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $expected = (
        "$($call.frame):$($call.input_x):$($call.output_a):" +
        "$($call.caller_pc)"
    )
    if (
        [int] $call.sequence -ne $index + 1 -or
        $expected -ne $expectedCalls[$index] -or
        $call.physical_bank -ne "00" -or
        $call.logical_pc -ne "E8B3" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "E8E6" -or
        [int64] $call.body_master_cycles -ne 282 -or
        [int] $call.write_count -ne 8
    ) {
        throw "BS-4 RNG call $($index + 1) changed."
    }
    if (-not $pathCounts.ContainsKey($call.caller_pc)) {
        $pathCounts[$call.caller_pc] = 0
    }
    ++$pathCounts[$call.caller_pc]
}
if (
    $pathCounts.Count -ne 2 -or
    $pathCounts["6B88"] -ne 18 -or
    $pathCounts["6CB1"] -ne 22
) {
    throw "BS-4 RNG consumer path counts changed."
}

# Reconstruct and verify every shared RNG update from its eight direct writes.
$directPairs = @(
    "E8B9:0042", "E8C0:0043", "E8C2:0042", "E8C4:0043",
    "E8CC:0042", "E8D3:0043", "E8DB:022B", "E8E0:022A"
)
foreach ($call in $calls) {
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
        throw "BS-4 RNG call $($call.sequence) changed write order."
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
                "BS-4 RNG recurrence changed in call $($call.sequence), " +
                "step $step."
            )
        }
    }
    if ([Convert]::ToInt32($call.output_a, 16) -ne $values[6]) {
        throw "BS-4 RNG call $($call.sequence) returned the wrong byte."
    }
}

$firstTimerResets = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "05" -and
            $_.instruction_logical_pc -eq "6B85"
        }
)
$secondTimerResets = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "05" -and
            $_.instruction_logical_pc -eq "6CAE"
        }
)
if (
    $firstTimerResets.Count -ne 18 -or
    $secondTimerResets.Count -ne 22 -or
    @($firstTimerResets | Where-Object { $_.value -ne "20" }).Count -ne 0 -or
    @($secondTimerResets | Where-Object { $_.value -ne "20" }).Count -ne 0
) {
    throw "BS-4 RNG timer reloads changed."
}

# The authentic source first allocates slot 66. Its four expiry calls use
# 6B88; after its hit continuation it moves to 6CA0 and contributes nine
# 6CB1 calls. Two later authentic allocations occupy slots 68 and 6D.
$expectedFirstResets = @(
    "1404:3B66", "1436:3B66", "1468:3B66", "1500:3B66",
    "1600:3B68", "1600:3B6D", "1632:3B68", "1632:3B6D",
    "1664:3B68", "1664:3B6D", "1696:3B68", "1696:3B6D",
    "1728:3B68", "1728:3B6D", "1760:3B68", "1760:3B6D",
    "1792:3B68", "1792:3B6D"
)
$expectedSecondResets = @(
    "1405:3B67", "1437:3B67", "1469:3B67", "1501:3B67",
    "1533:3B67", "1538:3B66", "1565:3B67", "1570:3B66",
    "1597:3B67", "1602:3B66", "1629:3B67", "1634:3B66",
    "1661:3B67", "1666:3B66", "1693:3B67", "1698:3B66",
    "1725:3B67", "1730:3B66", "1757:3B67", "1762:3B66",
    "1789:3B67", "1794:3B66"
)
$actualFirstResets = @(
    $firstTimerResets | ForEach-Object { "$($_.frame):$($_.logical_address)" }
)
$actualSecondResets = @(
    $secondTimerResets | ForEach-Object { "$($_.frame):$($_.logical_address)" }
)
if (
    ($actualFirstResets -join ",") -ne ($expectedFirstResets -join ",") -or
    ($actualSecondResets -join ",") -ne ($expectedSecondResets -join ",")
) {
    throw "BS-4 RNG actor/timer routing changed."
}

Write-Output "BS-4 linked-target RNG semantics verified."
Write-Output (
    "Consumer 6B88: 18 calls reload a 32-tick timer and pass RNG & 1F " +
    "to the motion helper."
)
Write-Output (
    "Consumer 6CB1: 22 calls use the same transform after allocation."
)
Write-Output (
    "E8B3: all 40 calls preserve the exact eight-write RNG recurrence."
)
