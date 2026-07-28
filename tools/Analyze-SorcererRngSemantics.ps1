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

# Hidden fixed slot $16 owns the eight Sorcerers. Its zero-live-count branch
# installs a 128-tick delay, waits for the Big Star to stop, selects collision
# selector $28/$29 from one RNG bit, reloads eight deployments, and returns
# through the ordinary controller loop.
Assert-RomHex $rom 0x00578E @"
A9 28 8D 0B 2D 9C 0F 2D A9 08 8D 10 2D 9E 80 39
9C 40 2D 20 18 C0 20 18 C0 9C 0B 2D 20 18 C0 AD
0F 2D D0 37 A9 80 20 21 C0 20 18 C0 AD 00 2D 0D
01 2D D0 0C 20 81 C0 A0 28 29 01 F0 0B C8 80 08
A0 28 AD 01 2D 10 01 C8 8C 0B 2D A9 08 8D 10 2D
20 18 C0 20 18 C0 9C 0B 2D 80 C1 AD 40 2D DD 80
39 F0 06 9D 80 39 20 3F 59 60
"@ "Sorcerer group controller and RNG selector"

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
    $calls.Count -ne 371 -or
    $writes.Count -ne 1764 -or
    $states.Count -ne 31028
) {
    throw "Sorcerer RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 58) {
    throw "Sorcerer RNG route shared-RNG population changed."
}

$expectedRngHistogram = @{
    "282,8" = 49
    "939,20" = 2
    "975,21" = 5
    "1029,22" = 1
    "1257,24" = 1
}
$actualRngHistogram = @{}
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
    throw "Sorcerer route RNG timing histogram changed."
}

$frames = @(
    1328, 1459, 1591, 1722, 1853, 1984, 2115, 2246, 2377,
    2508, 2639, 2770, 2901, 3032, 3163, 3294, 3425, 3556
)
$outputs = @(
    "15", "EA", "35", "3C", "6B", "B7", "DD", "15", "05",
    "9F", "BC", "E4", "B1", "EF", "FF", "11", "A5", "42"
)
$selectors = @(
    "29", "28", "29", "28", "29", "29", "29", "29", "29",
    "29", "28", "28", "29", "29", "29", "29", "29", "28"
)
$releaseCycles = @(
    636, 624, 636, 624, 636, 1611, 636, 1293, 636,
    636, 624, 1371, 636, 1329, 636, 1329, 636, 624
)
$consumerCycles = @(
    600, 588, 600, 588, 600, 1575, 600, 1257, 600,
    600, 588, 1335, 600, 1293, 600, 1293, 600, 588
)
$writeCounts = @(
    19, 19, 19, 19, 19, 35, 19, 31, 19,
    19, 19, 33, 19, 32, 19, 32, 19, 19
)

$releaseCalls = @($calls | Where-Object { $_.logical_pc -eq "57BA" })
$consumerCalls = @($calls | Where-Object { $_.logical_pc -eq "57C2" })
$targetRngCalls = @(
    $rngCalls |
        Where-Object {
            $_.caller_pc -eq "57C2" -and
            $_.input_x -eq "16"
        }
)
if (
    $releaseCalls.Count -ne 18 -or
    $consumerCalls.Count -ne 18 -or
    $targetRngCalls.Count -ne 18
) {
    throw "Sorcerer RNG consumer population changed."
}

for ($index = 0; $index -lt 18; ++$index) {
    $release = $releaseCalls[$index]
    $consumer = $consumerCalls[$index]
    $rng = $targetRngCalls[$index]
    if (
        [int] $release.frame -ne $frames[$index] -or
        $release.physical_bank -ne "02" -or
        $release.input_x -ne "16" -or
        $release.return_physical_bank -ne "01" -or
        $release.return_instruction_pc -ne "C5E9" -or
        [int64] $release.body_master_cycles -ne $releaseCycles[$index] -or
        [int] $release.write_count -ne $writeCounts[$index] -or
        [int] $consumer.frame -ne $frames[$index] -or
        $consumer.physical_bank -ne "02" -or
        $consumer.input_x -ne "16" -or
        $consumer.return_physical_bank -ne "01" -or
        $consumer.return_instruction_pc -ne "C5E9" -or
        [int64] $consumer.body_master_cycles -ne $consumerCycles[$index] -or
        [int] $consumer.write_count -ne $writeCounts[$index] -or
        [int] $rng.frame -ne $frames[$index] -or
        $rng.output_a -ne $outputs[$index]
    ) {
        throw "Sorcerer RNG call contract changed at frame $($frames[$index])."
    }

    $selectorWrites = @(
        $states |
            Where-Object {
                $_.frame -eq "$($frames[$index])" -and
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -eq "57D6" -and
                $_.logical_address -eq "2D0B"
            }
    )
    $deploymentWrites = @(
        $states |
            Where-Object {
                $_.frame -eq "$($frames[$index])" -and
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -eq "57DB" -and
                $_.logical_address -eq "2D10"
            }
    )
    $selectorClears = @(
        $states |
            Where-Object {
                $_.frame -eq "$($frames[$index] + 2)" -and
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -eq "57E4" -and
                $_.logical_address -eq "2D0B"
            }
    )
    if (
        $selectorWrites.Count -ne 1 -or
        $selectorWrites[0].value -ne $selectors[$index] -or
        $deploymentWrites.Count -ne 1 -or
        $deploymentWrites[0].value -ne "08" -or
        $selectorClears.Count -ne 1 -or
        $selectorClears[0].value -ne "00"
    ) {
        throw "Sorcerer selector cycle changed at frame $($frames[$index])."
    }
}

$branchFrames = @(
    1200, 1331, 1462, 1594, 1725, 1856, 1987, 2118, 2249, 2380,
    2511, 2642, 2773, 2904, 3035, 3166, 3297, 3428, 3559
)
$controllerBranches = @(
    $calls |
        Where-Object {
            $_.logical_pc -eq "57AD" -and
            [int64] $_.body_master_cycles -eq 363 -and
            [int] $_.write_count -eq 13
        }
)
if (
    $controllerBranches.Count -ne 19 -or
    (($controllerBranches.frame | ForEach-Object { [int] $_ }) -join ",") -ne
        ($branchFrames -join ",")
) {
    throw "Sorcerer zero-live controller cadence changed."
}

$expectedFirstCycle = @(
    "1200:01:C5FB:3F16:B7",
    "1200:01:C600:3F96:57",
    "1327:01:C5E1:3F16:BA",
    "1328:01:C5E1:3F16:E1",
    "1329:01:C5E1:3F16:E4",
    "1330:01:C5E1:3F16:AD",
    "1331:01:C5FB:3F16:B7"
)
foreach ($expected in $expectedFirstCycle) {
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
        throw "Sorcerer controller state write '$expected' changed."
    }
}

Write-Output "Sorcerer RNG semantics verified."
Write-Output (
    "Zero-live route: controller 02:57AD starts its 128-tick delay at frame 1200."
)
Write-Output (
    "RNG consumer 02:57C2: 18 events select collision values 28/29."
)
Write-Output (
    "Cadence: first selector at frame 1328, then a 131-frame controller cycle."
)
Write-Output (
    "Normalized samples: 02:57AD@259, 02:57BA@1, and 02:57C2@1 verified."
)
