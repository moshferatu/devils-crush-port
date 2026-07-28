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

function Assert-ScopeWrite {
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
        throw "$Label changed."
    }
}

function Select-StateWrites {
    param(
        [object[]] $States,
        [string] $Address,
        [string] $Bank,
        [string] $Pc
    )

    return @(
        $States |
            Where-Object {
                $_.logical_address -eq $Address -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc
            }
    )
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The left hand remains the authentic fixed slot-$40 actor. Only its Y
# coordinate is moved by the deterministic route so the existing camera
# activation sweep can dispatch its otherwise inactive live state.
Assert-RomHex $rom 0x004863 @"
00 40 72 59 48 00 37 00 14 C2 82 00
00 41 72 59 E8 00 37 00 15 C2 82 00
"@ "Upper Wall-Pocket hand records"

# Live state 5975 hides the collision surface, calls the shared RNG at 597D,
# masks the returned high byte with $3F, adds shared timer base $0080, and
# stores a 16-bit delay before yielding to countdown state 5991.
Assert-RomHex $rom 0x005975 @"
BD 80 30 09 40 9D 80 30 20 81 C0 29 3F 18 6D 80
00 9D 00 3B A9 01 9D 80 3B 20 18 C0 BD 00 3B D0
03 DE 80 3B 3A 9D 00 3B 1D 80 3B D0 52
"@ "Upper-hand live RNG and countdown entry"

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
    $calls.Count -ne 24 -or
    $writes.Count -ne 267 -or
    $states.Count -ne 4478
) {
    throw "Upper-hand RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 22) {
    throw "Upper-hand route RNG population changed."
}
foreach ($call in $rngCalls) {
    $expectedCycles = switch ([int] $call.sequence) {
        14 { 939 }
        15 { 1029 }
        default { 282 }
    }
    $expectedWriteCount = switch ([int] $call.sequence) {
        14 { 20 }
        15 { 22 }
        default { 8 }
    }
    if (
        $call.physical_bank -ne "00" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "E8E6" -or
        [int64] $call.body_master_cycles -ne $expectedCycles -or
        [int] $call.write_count -ne $expectedWriteCount
    ) {
        throw "RNG call $($call.sequence) changed its bounded contract."
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

$firstRng = $calls[0]
$live = $calls[1]
$consumer = $calls[2]
if (
    [int] $firstRng.sequence -ne 1 -or
    [int] $firstRng.frame -ne 1221 -or
    $firstRng.logical_pc -ne "E8B3" -or
    $firstRng.caller_pc -ne "597D" -or
    $firstRng.input_x -ne "40" -or
    $firstRng.output_a -ne "73" -or
    [int] $live.sequence -ne 2 -or
    [int] $live.frame -ne 1221 -or
    $live.physical_bank -ne "02" -or
    $live.logical_pc -ne "5975" -or
    $live.input_x -ne "40" -or
    $live.return_physical_bank -ne "01" -or
    $live.return_instruction_pc -ne "C5E9" -or
    [int64] $live.body_master_cycles -ne 1323 -or
    [int] $live.write_count -ne 33 -or
    [int] $consumer.sequence -ne 3 -or
    [int] $consumer.frame -ne 1221 -or
    $consumer.physical_bank -ne "02" -or
    $consumer.logical_pc -ne "597D" -or
    $consumer.input_x -ne "40" -or
    $consumer.return_physical_bank -ne "01" -or
    $consumer.return_instruction_pc -ne "C5E9" -or
    [int64] $consumer.body_master_cycles -ne 1287 -or
    [int] $consumer.write_count -ne 32
) {
    throw "Upper-hand live/consumer call contract changed."
}

# Result $73 masks to $33. The routine stores $0133, then the coroutine
# helper installs countdown continuation 02:5991.
Assert-ScopeWrite $writes 3 "02" "5986" "1B40" "33" `
    "Upper-hand timer low byte"
Assert-ScopeWrite $writes 3 "02" "598B" "1BC0" "01" `
    "Upper-hand timer high byte"
Assert-ScopeWrite $writes 3 "01" "C5E1" "1F40" "91" `
    "Upper-hand countdown pointer low"
Assert-ScopeWrite $writes 3 "01" "C5E6" "1FC0" "59" `
    "Upper-hand countdown pointer high"

$newSeedHigh = @(
    $writes |
        Where-Object {
            [int] $_.call_sequence -eq 1 -and
            $_.instruction_logical_pc -eq "E8DB" -and
            $_.work_ram_offset -eq "022B"
        }
)
$newSeedLow = @(
    $writes |
        Where-Object {
            [int] $_.call_sequence -eq 1 -and
            $_.instruction_logical_pc -eq "E8E0" -and
            $_.work_ram_offset -eq "022A"
        }
)
if (
    $newSeedHigh.Count -ne 1 -or
    $newSeedHigh[0].value -ne "73" -or
    $newSeedLow.Count -ne 1 -or
    $newSeedLow[0].value -ne "88"
) {
    throw "Upper-hand RNG seed result changed."
}

$activation = @(Select-StateWrites $states "30C0" "00" "F54B")
$timerLow = @(Select-StateWrites $states "3B40" "02" "5986")
$timerHigh = @(Select-StateWrites $states "3BC0" "02" "598B")
$countdownLow = @(Select-StateWrites $states "3B40" "02" "599A")
$countdownHigh = @(Select-StateWrites $states "3BC0" "02" "5996")
if (
    $activation.Count -ne 94 -or
    $activation[0].frame -ne "1220" -or
    $activation[0].value -ne "C2" -or
    $timerLow.Count -ne 1 -or
    $timerLow[0].frame -ne "1221" -or
    $timerLow[0].value -ne "33" -or
    $timerHigh.Count -ne 1 -or
    $timerHigh[0].frame -ne "1221" -or
    $timerHigh[0].value -ne "01" -or
    $countdownLow.Count -ne 299 -or
    $countdownLow[0].frame -ne "1222" -or
    $countdownLow[0].value -ne "32" -or
    $countdownLow[-1].frame -ne "1520" -or
    $countdownLow[-1].value -ne "08" -or
    $countdownHigh.Count -ne 1 -or
    $countdownHigh[0].frame -ne "1273" -or
    $countdownHigh[0].value -ne "00"
) {
    throw "Upper-hand activation or timer timeline changed."
}

Write-Output "Upper Wall-Pocket hand RNG semantics verified."
Write-Output (
    "Activation: slot 40 enters the camera window at frame 1220 and " +
    "dispatches 02:5975 at frame 1221."
)
Write-Output (
    "RNG consumer 02:597D: result 73 masks to 33 and installs timer 0133."
)
Write-Output (
    "RNG recurrence: all 22 E8B3 calls retain the eight direct writes; " +
    "20 are 282-cycle leaves and two include periodic interrupts."
)
Write-Output (
    "Countdown: 02:5991 decrements the routed timer through frame 1520."
)
