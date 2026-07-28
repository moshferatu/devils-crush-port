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
    [string] $StateWritePath
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

# The frame interrupt advances the free-running 16-bit seed once per frame.
Assert-RomBytes $rom 0x00016E @(
    0xEE, 0x2A, 0x22, 0xD0, 0x03, 0xEE, 0x2B, 0x22
) "IRQ RNG-seed tick"

# Bank 1 exposes the common RNG through a fixed JMP-table slot.
Assert-RomBytes $rom 0x002081 @(
    0x4C, 0xB3, 0xE8
) "C081 RNG trampoline"

# E8B3 mixes the old seed with 0x503F and frame counter 00, stores the new
# high and low bytes, and returns the new high byte in A.
Assert-RomBytes $rom 0x0008B3 @(
    0x18,
    0xAD, 0x2A, 0x22, 0x69, 0x3F, 0x85, 0x42,
    0xAD, 0x2B, 0x22, 0x69, 0x50, 0x85, 0x43,
    0x06, 0x42, 0x26, 0x43,
    0x18,
    0xAD, 0x2A, 0x22, 0x65, 0x00, 0x85, 0x42,
    0xAD, 0x2B, 0x22, 0x65, 0x43, 0x85, 0x43,
    0x18,
    0xAD, 0x2A, 0x22, 0x65, 0x43, 0x8D, 0x2B, 0x22,
    0xA5, 0x42, 0x8D, 0x2A, 0x22,
    0xAD, 0x2B, 0x22, 0x60
) "E8B3 RNG recurrence"

# The two active callers consume independent masked ranges from consecutive
# RNG results while initializing actor arrays.
Assert-RomBytes $rom 0x006678 @(
    0x20, 0x81, 0xC0, 0x29, 0x3F, 0x18, 0x69, 0x80,
    0x9D, 0x00, 0x3B
) "First active RNG consumer"
Assert-RomBytes $rom 0x006697 @(
    0x20, 0x81, 0xC0, 0x29, 0x1F, 0x1A, 0x9D, 0x80, 0x3B
) "Second active RNG consumer"

# The cartridge contains exactly 21 calls to the common RNG trampoline.
# Twelve belong to the normal-table banks and nine to bonus-stage banks.
# Only 6678/6697 are entered by the current state-census route, but every
# inactive consumer's immediate result transform remains part of the guarded
# native-port contract.
$consumerSignatures = @(
    [pscustomobject] @{
        Offset = 0x0045C9; Site = "02:45C9"; Class = "binary"
        Suffix = @(0x29, 0x01, 0xD0, 0x03, 0x4C, 0x0A, 0x45,
            0xA9, 0x02, 0x8D, 0x04, 0x2D)
    },
    [pscustomobject] @{
        Offset = 0x005148; Site = "02:5148"; Class = "masked"
        Suffix = @(0x29, 0x3F, 0x18, 0x69, 0x40, 0x20, 0x21, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x0057C2; Site = "02:57C2"; Class = "binary"
        Suffix = @(0xA0, 0x28, 0x29, 0x01, 0xF0, 0x0B, 0xC8, 0x80,
            0x08, 0xA0, 0x28, 0xAD, 0x01, 0x2D, 0x10, 0x01,
            0xC8, 0x8C, 0x0B, 0x2D)
    },
    [pscustomobject] @{
        Offset = 0x00597D; Site = "02:597D"; Class = "masked"
        Suffix = @(0x29, 0x3F, 0x18, 0x6D, 0x80, 0x00,
            0x9D, 0x00, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x0059F5; Site = "02:59F5"; Class = "masked"
        Suffix = @(0x29, 0x3F, 0x18, 0x69, 0xB0, 0x20, 0x21, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x006177; Site = "03:6177"; Class = "binary"
        Suffix = @(0xA0, 0x17, 0x29, 0x01, 0xF0, 0x02,
            0xA0, 0x19, 0x98, 0x20, 0x75, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x00625D; Site = "03:625D"; Class = "binary"
        Suffix = @(0xA0, 0x07, 0x29, 0x01, 0xF0, 0x02,
            0xA0, 0x09, 0x98, 0x9D, 0x80, 0x3C)
    },
    [pscustomobject] @{
        Offset = 0x006620; Site = "03:6620"; Class = "multi"
        Suffix = @(0x29, 0x03, 0x8D, 0x39, 0x2D, 0x1A, 0x29, 0x03,
            0x8D, 0x3A, 0x2D, 0x1A, 0x29, 0x03, 0x8D, 0x3B, 0x2D)
    },
    [pscustomobject] @{
        Offset = 0x006678; Site = "03:6678"; Class = "masked"
        Suffix = @(0x29, 0x3F, 0x18, 0x69, 0x80, 0x9D, 0x00, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x006697; Site = "03:6697"; Class = "masked"
        Suffix = @(0x29, 0x1F, 0x1A, 0x9D, 0x80, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x0068B1; Site = "03:68B1"; Class = "masked"
        Suffix = @(0x29, 0x1F, 0x18, 0x69, 0x40, 0x9D, 0x80, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x007834; Site = "03:7834"; Class = "binary"
        Suffix = @(0xA0, 0x07, 0x29, 0x01, 0xF0, 0x02,
            0xA0, 0x09, 0x98, 0x20, 0x75, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x0087C7; Site = "04:47C7"; Class = "masked"
        Suffix = @(0x29, 0x2F, 0x18, 0x69, 0x80, 0x9D, 0x00, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x008805; Site = "04:4805"; Class = "masked"
        Suffix = @(0x29, 0x2F, 0x18, 0x69, 0xC0, 0x9D, 0x00, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x00AB88; Site = "05:6B88"; Class = "masked"
        Suffix = @(0x29, 0x1F, 0x20, 0x75, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x00ACB1; Site = "05:6CB1"; Class = "masked"
        Suffix = @(0x29, 0x1F, 0x20, 0x75, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x00B217; Site = "05:7217"; Class = "masked"
        Suffix = @(0x29, 0x3F, 0x18, 0x69, 0x40, 0x9D, 0x00, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x00B32E; Site = "05:732E"; Class = "masked"
        Suffix = @(0x29, 0x1F, 0x18, 0x69, 0x18, 0x99, 0x00, 0x3B)
    },
    [pscustomobject] @{
        Offset = 0x00B33B; Site = "05:733B"; Class = "multi"
        Suffix = @(0x29, 0x0F, 0xA8, 0xB9, 0xB1, 0x73,
            0x20, 0x72, 0xC0)
    },
    [pscustomobject] @{
        Offset = 0x00E8C7; Site = "07:48C7"; Class = "multi"
        Suffix = @(0x29, 0x0F, 0x20, 0xDF, 0x48)
    },
    [pscustomobject] @{
        Offset = 0x00E8CF; Site = "07:48CF"; Class = "binary"
        Suffix = @(0xA0, 0x2C, 0x29, 0x01, 0xF0, 0x02,
            0xA0, 0x2D, 0x98, 0x20, 0x87, 0xC0)
    }
)

$foundConsumerOffsets = [System.Collections.Generic.List[int]]::new()
for ($offset = 0; $offset -le $rom.Count - 3; ++$offset) {
    if (
        $rom[$offset] -eq 0x20 -and
        $rom[$offset + 1] -eq 0x81 -and
        $rom[$offset + 2] -eq 0xC0
    ) {
        $foundConsumerOffsets.Add($offset)
    }
}
$expectedConsumerOffsets = @($consumerSignatures | ForEach-Object {
    [int] $_.Offset
})
if (
    $foundConsumerOffsets.Count -ne 21 -or
    ($foundConsumerOffsets -join ",") -ne
        ($expectedConsumerOffsets -join ",")
) {
    throw "RNG consumer call-site census changed."
}
foreach ($consumer in $consumerSignatures) {
    [byte[]] $signature = @(
        0x20, 0x81, 0xC0
    ) + @($consumer.Suffix)
    Assert-RomBytes $rom $consumer.Offset $signature (
        "RNG consumer $($consumer.Site)"
    )
}

$normalConsumers = @(
    $consumerSignatures |
        Where-Object { $_.Site.StartsWith("02:") -or $_.Site.StartsWith("03:") }
)
$bonusConsumers = @(
    $consumerSignatures |
        Where-Object { $_.Site.StartsWith("04:") -or
            $_.Site.StartsWith("05:") -or $_.Site.StartsWith("07:") }
)
$classCounts = @{}
foreach ($group in @($consumerSignatures | Group-Object -Property Class)) {
    $classCounts[$group.Name] = $group.Count
}
if (
    $normalConsumers.Count -ne 12 -or
    $bonusConsumers.Count -ne 9 -or
    $classCounts["masked"] -ne 12 -or
    $classCounts["binary"] -ne 6 -or
    $classCounts["multi"] -ne 3
) {
    throw "RNG consumer bank or transform classification changed."
}

$stateWrites = Import-TraceCsv $StateWritePath
if ($stateWrites.Count -ne 3771) {
    throw "Expected 3,771 selected RNG-state writes."
}
$expectedStateWriterCounts = @{
    "01:C100:222A" = 1
    "01:C100:222B" = 1
    "00:E16E:222A" = 3599
    "00:E173:222B" = 14
    "00:E8DB:222B" = 78
    "00:E8E0:222A" = 78
}
$actualStateWriterCounts = @{}
foreach ($write in $stateWrites) {
    $key = (
        "$($write.instruction_physical_bank):" +
        "$($write.instruction_logical_pc):$($write.logical_address)"
    )
    if (-not $actualStateWriterCounts.ContainsKey($key)) {
        $actualStateWriterCounts[$key] = 0
    }
    ++$actualStateWriterCounts[$key]
}
if ($actualStateWriterCounts.Count -ne $expectedStateWriterCounts.Count) {
    throw "RNG-state writer set changed."
}
foreach ($key in $expectedStateWriterCounts.Keys) {
    if (
        -not $actualStateWriterCounts.ContainsKey($key) -or
        $actualStateWriterCounts[$key] -ne $expectedStateWriterCounts[$key]
    ) {
        throw "RNG-state writer count changed for $key."
    }
}
$ticks = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "E16E"
        }
)
for ($index = 0; $index -lt $ticks.Count; ++$index) {
    if ([int] $ticks[$index].frame -ne $index + 2) {
        throw "The RNG seed was not ticked exactly once in frame $($index + 2)."
    }
}

$calls = Import-TraceCsv $CallPath
if ($calls.Count -ne 78) {
    throw "Expected 78 RNG calls, found $($calls.Count)."
}
$expectedPaths = @{
    "6678:667B" = 9
    "6697:669A" = 69
}
$actualPaths = @{}
foreach ($call in $calls) {
    if (
        $call.physical_bank -ne "00" -or
        $call.logical_pc -ne "E8B3" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "E8E6"
    ) {
        throw "RNG call $($call.sequence) changed its bounded path."
    }
    $key = "$($call.caller_pc):$($call.return_pc)"
    if (-not $actualPaths.ContainsKey($key)) {
        $actualPaths[$key] = 0
    }
    ++$actualPaths[$key]
}
foreach ($key in $expectedPaths.Keys) {
    if (
        -not $actualPaths.ContainsKey($key) -or
        $actualPaths[$key] -ne $expectedPaths[$key]
    ) {
        throw "RNG caller path count changed for $key."
    }
}

$writes = Import-TraceCsv $WritePath
$directPairs = @(
    "E8B9:0042",
    "E8C0:0043",
    "E8C2:0042",
    "E8C4:0043",
    "E8CC:0042",
    "E8D3:0043",
    "E8DB:022B",
    "E8E0:022A"
)
foreach ($call in $calls) {
    $directWrites = @(
        $writes |
            Where-Object {
                $_.call_sequence -eq $call.sequence -and
                $directPairs -contains (
                    "$($_.instruction_logical_pc):$($_.work_ram_offset)"
                )
            }
    )
    if ($directWrites.Count -ne 8) {
        throw "RNG call $($call.sequence) did not have eight direct writes."
    }
    $actualPairs = @(
        $directWrites |
            ForEach-Object {
                "$($_.instruction_logical_pc):$($_.work_ram_offset)"
            }
    )
    if (($actualPairs -join ",") -ne ($directPairs -join ",")) {
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
    $shiftedLow = ($values[0] -shl 1) -band 0xFF
    $shiftedHigh = (
        ($values[1] -shl 1) + ($values[0] -shr 7)
    ) -band 0xFF
    $newLow = ($oldLow + $frameCounter) -band 0xFF
    $expected = @(
        $values[0],
        $values[1],
        $shiftedLow,
        $shiftedHigh,
        $newLow,
        0,
        0,
        0
    )
    $expected[5] = (
        $oldHigh + $expected[3] + $frameCarry
    ) -band 0xFF
    $expected[6] = ($oldLow + $expected[5]) -band 0xFF
    $expected[7] = $expected[4]
    for ($index = 2; $index -lt $values.Count; ++$index) {
        if ($values[$index] -ne $expected[$index]) {
            throw (
                "RNG recurrence changed in call $($call.sequence), " +
                "step $index."
            )
        }
    }
    if (
        [Convert]::ToInt32($call.output_a, 16) -ne $values[6]
    ) {
        throw "RNG call $($call.sequence) did not return its new high byte."
    }
}

Write-Output "RNG semantics verified."
Write-Output (
    "Seed 222B:222A ticks once per frame and is mixed with 0x503F " +
    "and frame counter 00."
)
Write-Output (
    "E8B3: 78 calls follow the exact eight-write recurrence and return " +
    "the new seed high byte."
)
Write-Output "Active RNG consumers: 6678=9 calls; 6697=69 calls."
Write-Output (
    "RNG consumer census: 21 sites; 12 normal-table and 9 bonus-stage."
)
Write-Output (
    "Consumer transforms: 12 masked timers/offsets, 6 binary selectors, " +
    "3 multi-way selectors; 19 sites remain dynamically inactive."
)
