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

# Slot $46 is the left member of the first fixed Little-Skull pair.
Assert-RomHex $rom 0x0048AB @"
00 46 E4 44 6C 00 3E 00 24 82 82 00
"@ "Little-Skull slot 46 layout record"

# Gray state 458A hides the collision surface and returns through the common
# continuation helper. Once the shared latch is clear, 45A4 restores the
# live fields, raises the left-pair pending bit, and calls the RNG at 45C9.
# The result bit selects pending value one or two before the actor returns to
# its ordinary live state at 450A/4515.
Assert-RomHex $rom 0x00458A @"
A9 0B 9D 00 3D BD 80 30 09 40 9D 80 30 A9 38 9D
00 33 A9 00 9D 80 33 20 18 C0 AD FA 2C F0 01 60
BD 80 30 29 BF 9D 80 30 BD 00 38 29 7F 9D 00 38
A9 03 9D 00 3D A9 04 9D 00 3B A9 01 8D 04 2D 20
81 C0 29 01 D0 03 4C 0A 45 A9 02 8D 04 2D 4C 0A
45
"@ "Little-Skull gray-state release and RNG selection"

Assert-RomHex $rom 0x002081 "4C B3 E8" "Shared RNG trampoline"
Assert-RomHex $rom 0x0008B3 @"
18 AD 2A 22 69 3F 85 42 AD 2B 22 69 50 85 43 06
42 26 43 18 AD 2A 22 65 00 85 42 AD 2B 22 65 43
85 43 18 AD 2A 22 65 43 8D 2B 22 A5 42 8D 2A 22
AD 2B 22 60
"@ "Shared E8B3 RNG recurrence"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 131 -or
    $writes.Count -ne 1268 -or
    $states.Count -ne 5274
) {
    throw "Little-Skull RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 128) {
    throw "Little-Skull route RNG population changed."
}

$expectedRngHistogram = @{
    "282,8" = 114
    "939,20" = 4
    "975,21" = 5
    "1029,22" = 1
    "1167,22" = 1
    "1257,24" = 1
    "1293,25" = 1
    "1314,26" = 1
}
$actualRngHistogram = @{}
foreach ($call in $rngCalls) {
    if (
        $call.physical_bank -ne "00" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "E8E6"
    ) {
        throw "RNG call $($call.sequence) changed its bounded identity."
    }

    $histogramKey = "{0},{1}" -f (
        [int64] $call.body_master_cycles
    ), [int] $call.write_count
    if (-not $actualRngHistogram.ContainsKey($histogramKey)) {
        $actualRngHistogram[$histogramKey] = 0
    }
    ++$actualRngHistogram[$histogramKey]

    $expectedPairs = @(
        "E8B9:0042",
        "E8C0:0043",
        "E8C2:0042",
        "E8C4:0043",
        "E8CC:0042",
        "E8D3:0043",
        "E8DB:022B",
        "E8E0:022A"
    )
    $callWrites = @(
        $writes |
            Where-Object {
                [int] $_.call_sequence -eq [int] $call.sequence -and
                $expectedPairs -contains (
                    "$($_.instruction_logical_pc):$($_.work_ram_offset)"
                )
            }
    )
    $actualPairs = @(
        $callWrites |
            ForEach-Object {
                "$($_.instruction_logical_pc):$($_.work_ram_offset)"
            }
    )
    if (($actualPairs -join ",") -ne ($expectedPairs -join ",")) {
        throw "RNG call $($call.sequence) changed direct write order."
    }
}
if (
    $actualRngHistogram.Count -ne $expectedRngHistogram.Count -or
    @(
        $expectedRngHistogram.GetEnumerator() |
            Where-Object {
                -not $actualRngHistogram.ContainsKey($_.Key) -or
                $actualRngHistogram[$_.Key] -ne $_.Value
            }
    ).Count -ne 0
) {
    throw "Little-Skull route RNG timing histogram changed."
}

$gray = @($calls | Where-Object { $_.logical_pc -eq "458A" })
$release = @($calls | Where-Object { $_.logical_pc -eq "45A4" })
$consumer = @($calls | Where-Object { $_.logical_pc -eq "45C9" })
$targetRng = @(
    $rngCalls |
        Where-Object {
            $_.frame -eq "1223" -and
            $_.caller_pc -eq "45C9" -and
            $_.input_x -eq "46"
        }
)
if (
    $gray.Count -ne 1 -or
    [int] $gray[0].sequence -ne 1 -or
    [int] $gray[0].frame -ne 1221 -or
    $gray[0].physical_bank -ne "02" -or
    $gray[0].input_x -ne "46" -or
    $gray[0].return_physical_bank -ne "01" -or
    $gray[0].return_instruction_pc -ne "C5E9" -or
    [int64] $gray[0].body_master_cycles -ne 291 -or
    [int] $gray[0].write_count -ne 11 -or
    $release.Count -ne 1 -or
    [int] $release[0].sequence -ne 3 -or
    [int] $release[0].frame -ne 1223 -or
    $release[0].physical_bank -ne "02" -or
    $release[0].input_x -ne "46" -or
    $release[0].return_physical_bank -ne "01" -or
    $release[0].return_instruction_pc -ne "C5E9" -or
    [int64] $release[0].body_master_cycles -ne 768 -or
    [int] $release[0].write_count -ne 23 -or
    $consumer.Count -ne 1 -or
    [int] $consumer[0].sequence -ne 4 -or
    [int] $consumer[0].frame -ne 1223 -or
    $consumer[0].physical_bank -ne "02" -or
    $consumer[0].input_a -ne "01" -or
    $consumer[0].input_x -ne "46" -or
    $consumer[0].output_a -ne "45" -or
    $consumer[0].output_status -ne "41" -or
    $consumer[0].return_physical_bank -ne "01" -or
    $consumer[0].return_instruction_pc -ne "C5E9" -or
    [int64] $consumer[0].body_master_cycles -ne 606 -or
    [int] $consumer[0].write_count -ne 18 -or
    $targetRng.Count -ne 1 -or
    [int] $targetRng[0].sequence -ne 2 -or
    [int64] $targetRng[0].body_master_cycles -ne 282 -or
    [int] $targetRng[0].write_count -ne 8 -or
    $targetRng[0].output_a -ne "75"
) {
    throw "Little-Skull gray-release or RNG call contract changed."
}

$expectedStateWrites = @(
    "1220:00:F54B:30C6:82",
    "1221:02:458C:3D46:0B",
    "1221:02:4594:30C6:C2",
    "1221:02:4599:3346:38",
    "1221:02:459E:33C6:00",
    "1221:01:C5E1:3F46:A4",
    "1221:01:C5E6:3FC6:45",
    "1222:00:F54B:30C6:C2",
    "1223:02:45AF:30C6:82",
    "1223:02:45B7:3846:10",
    "1223:02:45BC:3D46:03",
    "1223:02:45C1:3B46:04",
    "1223:02:45C6:2D04:01",
    "1223:00:E8DB:222B:75",
    "1223:00:E8E0:222A:8C",
    "1223:02:45D5:2D04:02",
    "1223:01:C5E1:3F46:15",
    "1223:01:C5E6:3FC6:45"
)
foreach ($expected in $expectedStateWrites) {
    $parts = $expected.Split(":")
    $matches = @(
        $states |
            Where-Object {
                $_.frame -eq $parts[0] -and
                $_.instruction_physical_bank -eq $parts[1] -and
                $_.instruction_logical_pc -eq $parts[2] -and
                $_.logical_address -eq $parts[3] -and
                $_.value -eq $parts[4]
            }
    )
    if ($matches.Count -ne 1) {
        throw "Little-Skull state write '$expected' changed."
    }
}

Write-Output "Little-Skull RNG semantics verified."
Write-Output (
    "Activation: fixed slot 46 enters gray state 02:458A at frame 1221."
)
Write-Output (
    "RNG consumer 02:45C9: result 75 selects left-pair pending value 2."
)
Write-Output (
    "Call contract: release 02:45A4 is 768 cycles and consumer is 606 cycles."
)
Write-Output (
    "Normalized samples: 02:45A4@1 and 02:45C9@1 verified at frame 1223."
)
