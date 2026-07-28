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

Assert-RomHex $rom 0x004923 @"
02 50 76 68 28 00 4E 00 41 82 83 00
02 51 76 68 2E 00 42 00 41 82 83 01
02 52 76 68 36 00 32 00 41 82 83 02
02 53 76 68 42 00 22 00 41 82 83 02
02 54 76 68 5A 00 1E 00 41 82 83 02
02 55 76 68 72 00 1E 00 41 82 83 02
02 56 76 68 8A 00 1E 00 41 82 83 02
"@ "Gray-Blob fixed layout row"

# Every first live update clears transient flags, calls the common RNG, masks
# to five bits, adds $40, and stores the resulting movement timer.
Assert-RomHex $rom 0x0068A1 @"
BD 80 30 29 9F 9D 80 30 BD 00 38 29 7F 9D 00 38
20 81 C0 29 1F 18 69 40 9D 80 3B 20 18 C0 DE 00
3A D0 0D A9 04 9D 00 3A BD 80 39 49 01 9D 80 39
BD 00 38 10 03 4C 06 69 DE 80 3B D0 27 38 62 FD
00 34 9D 00 34 62 FD 80 34 9D 80 34 A9 06 9D 00
3B 20 18 C0 20 12 C0 BD 00 38 30 09 DE 00 3B D0
03 4C A1 68 60
"@ "Gray-Blob live movement and RNG loop"

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
    $calls.Count -ne 76 -or
    $writes.Count -ne 762 -or
    $states.Count -ne 3866
) {
    throw "Gray-Blob RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 62) {
    throw "Gray-Blob route RNG population changed."
}
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
        $call.return_instruction_pc -ne "E8E6" -or
        [int64] $call.body_master_cycles -ne 282 -or
        [int] $call.write_count -ne 8
    ) {
        throw "RNG call $($call.sequence) changed its bounded contract."
    }
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

$liveCalls = @($calls | Where-Object { $_.logical_pc -eq "68A1" })
$consumerCalls = @($calls | Where-Object { $_.logical_pc -eq "68B1" })
$blobRngCalls = @($rngCalls | Where-Object { $_.caller_pc -eq "68B1" })
$expectedSlots = @("50", "51", "52", "53", "54", "55", "56")
$expectedFrames = @(1541, 1542, 1544, 1546, 1547, 1547, 1547)
$expectedRngSequences = @(7, 10, 15, 18, 21, 24, 27)
$expectedLiveSequences = @(8, 11, 16, 19, 22, 25, 28)
$expectedConsumerSequences = @(9, 12, 17, 20, 23, 26, 29)
$expectedOutputs = @("46", "10", "47", "30", "F6", "52", "70")
$expectedTimers = @("46", "50", "47", "50", "56", "52", "50")
if (
    $liveCalls.Count -ne 7 -or
    $consumerCalls.Count -ne 7 -or
    $blobRngCalls.Count -ne 7
) {
    throw "Gray-Blob live or consumer population changed."
}

for ($index = 0; $index -lt 7; ++$index) {
    $live = $liveCalls[$index]
    $consumer = $consumerCalls[$index]
    $rng = $blobRngCalls[$index]
    if (
        [int] $rng.sequence -ne $expectedRngSequences[$index] -or
        [int] $live.sequence -ne $expectedLiveSequences[$index] -or
        [int] $consumer.sequence -ne $expectedConsumerSequences[$index] -or
        [int] $rng.frame -ne $expectedFrames[$index] -or
        [int] $live.frame -ne $expectedFrames[$index] -or
        [int] $consumer.frame -ne $expectedFrames[$index] -or
        $rng.input_x -ne $expectedSlots[$index] -or
        $rng.output_a -ne $expectedOutputs[$index] -or
        $live.physical_bank -ne "03" -or
        $live.input_x -ne $expectedSlots[$index] -or
        $live.return_physical_bank -ne "01" -or
        $live.return_instruction_pc -ne "C5E9" -or
        [int64] $live.body_master_cycles -ne 633 -or
        [int] $live.write_count -ne 20 -or
        $consumer.physical_bank -ne "03" -or
        $consumer.input_x -ne $expectedSlots[$index] -or
        $consumer.return_physical_bank -ne "01" -or
        $consumer.return_instruction_pc -ne "C5E9" -or
        [int64] $consumer.body_master_cycles -ne 561 -or
        [int] $consumer.write_count -ne 18
    ) {
        throw "Gray-Blob RNG call contract changed for slot $($expectedSlots[$index])."
    }

    $timerAddress = "3BD{0:X1}" -f $index
    $timerWrites = @(
        $states |
            Where-Object {
                $_.frame -eq "$($expectedFrames[$index])" -and
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq "68B9" -and
                $_.logical_address -eq $timerAddress
            }
    )
    if (
        $timerWrites.Count -ne 1 -or
        $timerWrites[0].value -ne $expectedTimers[$index]
    ) {
        throw "Gray-Blob movement timer changed for slot $($expectedSlots[$index])."
    }
}

$flagClears = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "68A6" -and
            $_.logical_address -in @(
                "30D0", "30D1", "30D2", "30D3", "30D4", "30D5", "30D6"
            )
        }
)
$collisionClears = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "68AE" -and
            $_.logical_address -in @(
                "3850", "3851", "3852", "3853", "3854", "3855", "3856"
            )
        }
)
if (
    $flagClears.Count -ne 7 -or
    ($flagClears.value | Select-Object -Unique) -ne "82" -or
    $collisionClears.Count -ne 7 -or
    ($collisionClears.value | Select-Object -Unique) -ne "13"
) {
    throw "Gray-Blob first-live flag clearing changed."
}

Write-Output "Gray-Blob RNG semantics verified."
Write-Output (
    "Natural activation: slots 50-56 enter 03:68A1 across frames 1541-1547."
)
Write-Output (
    "RNG consumer 03:68B1: seven results produce timers 46,50,47,50,56,52,50."
)
Write-Output (
    "Call contract: every first live update is 633 cycles and 20 writes."
)
Write-Output (
    "First normalized sample: 03:68A1@1, frame 1541, 633 cycles."
)
