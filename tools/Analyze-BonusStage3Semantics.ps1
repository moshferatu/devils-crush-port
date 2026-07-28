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
    [string] $StatePath,

    [string] $TimeoutStatePath,

    [string] $CompletionStatePath,

    [switch] $SkipResults
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

function Assert-Write {
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

function Select-StateWrites {
    param(
        [object[]] $States,
        [string] $Address,
        [string] $Pc
    )

    return @(
        $States |
            Where-Object {
                $_.logical_address -eq $Address -and
                $_.instruction_physical_bank -eq "04" -and
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

# BS-3 has three emitter records followed by three eight-hit objective
# records. Their record parameter is the lane/objective index 0, 1, or 2.
Assert-RomHex $rom 0x0081C1 @"
00 0B AC 53 00 00 20 00 00 60 83 00
00 0C CD 53 3E 00 30 00 1D E0 82 00
00 0D CD 53 F8 00 30 00 1D E0 82 01
00 0E CD 53 FA 00 30 00 1D E0 82 02
00 0F DF 55 50 00 70 00 26 C0 82 00
00 10 DF 55 98 00 40 00 26 C0 82 01
00 11 DF 55 E0 00 70 00 26 C0 82 02
"@ "BS-3 controller, emitter, and objective records"

# The controller owns three live-objective bytes. Once all three are zero it
# selects multiplier $14 (20) and enters the shared completion path.
Assert-RomHex $rom 0x0093AC @"
A9 01 8D 2F 2D 8D 30 2D 8D 31 2D 20 18 C0 AD 2F
2D 0D 30 2D 0D 31 2D D0 07 A9 14 85 CA 4C DB 44
60
"@ "BS-3 completion controller"

# Each emitter owns a six-spawn batch and a 29-tick cadence. Allocated
# type-$C2 targets copy the lane, position, and appearance, then increment
# the matching live-target byte at $2D28-$2D2A.
Assert-RomHex $rom 0x0093CD @"
9D 00 37 BC 00 37 62 99 28 2D A9 01 9D 00 3A A9
06 9D 00 3B 20 18 C0 BC 00 37 B9 2F 2D D0 03 4C
27 C0 DE 00 3A D0 0D A9 1D 9D 00 3A 20 24 54 DE
00 3B F0 01 60
"@ "BS-3 three-lane emitter"
Assert-RomHex $rom 0x009424 @"
20 24 C0 90 01 60 A9 C2 99 00 30 BD 80 31 99 80
31 BD 00 32 99 00 32 BD 00 33 99 00 33 BD 80 33
99 80 33 BD 00 39 99 00 39 BD 00 37 99 00 37 A9
C0 99 80 30 A9 10 99 80 3D A9 10 99 00 38 A9 81
99 00 3D A9 02 99 00 3C A9 81 99 00 3F A9 54 99
80 3F BC 00 37 B9 28 2D 1A 99 28 2D 60
"@ "BS-3 emitted-target allocator"

# A live emitted target contact awards 100 stored units, decrements its
# lane's live count, emits the common impact effect, and releases the slot.
Assert-RomHex $rom 0x0094E7 @"
BD 00 38 30 01 60 A9 64 85 2F 64 30 20 2A C0 BC
00 37 B9 28 2D 3A 99 28 2D 20 9E 66 4C 27 C0
"@ "BS-3 emitted-target contact"

# Each objective takes eight hits. Every hit awards 100 stored units and
# command $26. The eighth hit adds 1,000 stored units, runs its destruction
# effect, and clears the indexed objective byte after the final animation.
Assert-RomHex $rom 0x0095DF @"
9D 00 37 A9 81 9D 00 3D A9 10 9D 00 38 A9 08 9D
00 3C
"@ "BS-3 objective initializer"
Assert-RomHex $rom 0x00966C @"
BD 00 38 10 1D 29 7F 9D 00 38 A9 64 85 2F 64 30
20 2A C0 A9 26 20 87 C0 DE 00 3C D0 02 38 60 20
B9 68 18 60 A9 E8 85 2F A9 03 85 30 20 2A C0 20
F6 65 A9 06 20 21 C0 20 18 C0 BC 00 37 62 99 2F
2D 4C 27 C0
"@ "BS-3 objective hit and destruction"

# The shared result builder consumes the controller's multiplier, adding
# 1,000 stored units once per count.
Assert-RomHex $rom 0x008A71 @"
A4 CA 18 AD B7 2D 69 E8 8D B7 2D AD B8 2D 69 03
8D B8 2D 62 6D B9 2D 8D B9 2D 88 D0 E5 60
"@ "Bonus special-award builder"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
$timeoutStates = @()
$completionStates = @()
if (-not $SkipResults) {
    if (
        [string]::IsNullOrWhiteSpace($TimeoutStatePath) -or
        [string]::IsNullOrWhiteSpace($CompletionStatePath)
    ) {
        throw "TimeoutStatePath and CompletionStatePath are required."
    }
    $timeoutStates = @(Import-TraceCsv $TimeoutStatePath)
    $completionStates = @(Import-TraceCsv $CompletionStatePath)
}
if (
    $calls.Count -ne 18 -or
    $writes.Count -ne 1003 -or
    $states.Count -ne 1120 -or
    (
        -not $SkipResults -and
        (
            $timeoutStates.Count -ne 1164 -or
            $completionStates.Count -ne 2328
        )
    )
) {
    throw "BS-3 event/result artifact populations changed."
}

$expectedCalls = @(
    @("53CD", "34", "C5E9", 285, 11),
    @("53CD", "35", "C5E9", 285, 11),
    @("53CD", "50", "C5E9", 285, 11),
    @("55DF", "51", "C5E9", 306, 12),
    @("55DF", "52", "C5E9", 306, 12),
    @("55DF", "53", "C5E9", 306, 12),
    @("5424", "34", "5480", 4344, 19),
    @("53E4", "34", "5401", 4503, 24),
    @("5424", "35", "5480", 4389, 19),
    @("53E4", "35", "5401", 4548, 24),
    @("5424", "50", "5480", 4434, 19),
    @("53E4", "50", "5401", 4593, 24),
    @("53E4", "34", "5401", 75, 1),
    @("53E4", "35", "5401", 75, 1),
    @("53E4", "50", "5401", 75, 1),
    @("5626", "51", "C5E9", 45957, 503),
    @("56A6", "52", "C594", 4344, 112),
    @("54E7", "58", "C594", 11094, 187)
)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $expected = $expectedCalls[$index]
    $expectedFrame = if ($index -lt 12) { 1342 } else { 1404 }
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne $expectedFrame -or
        $call.physical_bank -ne "04" -or
        $call.logical_pc -ne $expected[0] -or
        $call.input_x -ne $expected[1] -or
        $call.return_instruction_pc -ne $expected[2] -or
        [int64] $call.body_master_cycles -ne $expected[3] -or
        [int] $call.write_count -ne $expected[4]
    ) {
        throw "BS-3 event call $($index + 1) changed."
    }
}

# The three emitters allocate one target apiece at slots $58-$5A and balance
# independent live-target counts.
Assert-Write $writes 7 "04" "542C" "1058" "C2" `
    "First emitted target type"
Assert-Write $writes 7 "04" "5450" "1758" "00" `
    "First emitted target lane"
Assert-Write $writes 7 "04" "547D" "0D28" "01" `
    "First emitted target live count"
Assert-Write $writes 9 "04" "542C" "1059" "C2" `
    "Second emitted target type"
Assert-Write $writes 9 "04" "5450" "1759" "01" `
    "Second emitted target lane"
Assert-Write $writes 9 "04" "547D" "0D29" "01" `
    "Second emitted target live count"
Assert-Write $writes 11 "04" "542C" "105A" "C2" `
    "Third emitted target type"
Assert-Write $writes 11 "04" "5450" "175A" "02" `
    "Third emitted target lane"
Assert-Write $writes 11 "04" "547D" "0D2A" "01" `
    "Third emitted target live count"

# Slot $51 takes its eighth hit: the normal 100-unit award is followed by the
# 1,000-unit destruction award. Slot $52 exercises the indexed finalizer.
Assert-Write $writes 16 "04" "5673" "1851" "10" `
    "Objective collision clear"
Assert-Write $writes 16 "00" "F56B" "0DB7" "64" `
    "Objective hit award"
Assert-Write $writes 16 "00" "ECFD" "002A" "26" `
    "Objective hit audio command"
Assert-Write $writes 16 "04" "5684" "1C51" "00" `
    "Objective terminal counter"
Assert-Write $writes 16 "00" "F56B" "0DB7" "4C" `
    "Objective destruction award"
Assert-Write $writes 17 "04" "56AA" "0D30" "00" `
    "Second objective finalization"

# The first emitted target adds a further 100 stored units, balances its
# live-target count, and releases through impact selector $2D.
Assert-Write $writes 18 "00" "F56B" "0DB7" "B0" `
    "Emitted-target contact award"
Assert-Write $writes 18 "04" "54FD" "0D28" "00" `
    "Emitted-target live-count balance"
Assert-Write $writes 18 "05" "66A4" "0D6E" "2D" `
    "Emitted-target impact selector"

$bonusLow = @(
    $states |
        Where-Object {
            $_.logical_address -eq "2DB7" -and
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "F56B" -and
            $_.frame -eq "1404"
        }
)
$objectiveFinalizers = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "56AA"
        }
)
if (
    $bonusLow.Count -ne 3 -or
    ($bonusLow.value -join ",") -ne "64,4C,B0" -or
    $objectiveFinalizers.Count -ne 1 -or
    $objectiveFinalizers[0].logical_address -ne "2D30" -or
    $objectiveFinalizers[0].value -ne "00"
) {
    throw "BS-3 event score/objective timeline changed."
}

if (-not $SkipResults) {
    # The unmodified stage reaches the shared zero-progress timeout. It
    # advances 60 result glyphs and builds no special award.
    $timeoutTicks = @(Select-StateWrites $timeoutStates "2D55" "4DC4")
    $timeoutAdvances = @(Select-StateWrites $timeoutStates "2D67" "4DD7")
    if (
        $timeoutTicks.Count -ne 240 -or
        $timeoutAdvances.Count -ne 60 -or
        $timeoutAdvances[0].frame -ne "1529" -or
        $timeoutAdvances[0].value -ne "01" -or
        $timeoutAdvances[-1].frame -ne "1765" -or
        $timeoutAdvances[-1].value -ne "3C" -or
        @(Select-StateWrites $timeoutStates "2DB7" "4A79").Count -ne 0 -or
        @(Select-StateWrites $timeoutStates "2DB7" "46DD").Count -ne 0
    ) {
        throw "BS-3 timeout presentation changed."
    }

    # The targeted all-clear state retains the real controller and its
    # multiplier $14. The shared builder adds 1,000 twenty times and
    # transfers 20,000 stored units in 40 chunks of 500.
    $completionTicks = @(
        Select-StateWrites $completionStates "2D55" "4DC4"
    )
    $completionAdvances = @(
        Select-StateWrites $completionStates "2D67" "4DD7"
    )
    $awardLow = @(Select-StateWrites $completionStates "2DB7" "4A79")
    $awardMiddle = @(Select-StateWrites $completionStates "2DB8" "4A81")
    $awardHigh = @(Select-StateWrites $completionStates "2DB9" "4A88")
    $transferLow = @(Select-StateWrites $completionStates "2DB7" "46DD")
    $transferMiddle = @(
        Select-StateWrites $completionStates "2DB8" "46E2"
    )
    $transferHigh = @(Select-StateWrites $completionStates "2DB9" "46E7")
    $handshake = @(Select-StateWrites $completionStates "2D66" "473A")
    if (
        $completionTicks.Count -ne 402 -or
        $completionAdvances.Count -ne 101 -or
        $completionAdvances[0].frame -ne "1529" -or
        $completionAdvances[0].value -ne "01" -or
        $completionAdvances[-1].frame -ne "1929" -or
        $completionAdvances[-1].value -ne "65" -or
        $awardLow.Count -ne 20 -or
        $awardMiddle.Count -ne 20 -or
        $awardHigh.Count -ne 20 -or
        $awardLow[-1].value -ne "20" -or
        $awardMiddle[-1].value -ne "4E" -or
        $awardHigh[-1].value -ne "00" -or
        $transferLow.Count -ne 40 -or
        $transferMiddle.Count -ne 40 -or
        $transferHigh.Count -ne 40 -or
        $transferLow[0].frame -ne "1830" -or
        $transferLow[0].value -ne "2C" -or
        $transferMiddle[0].value -ne "4C" -or
        $transferHigh[0].value -ne "00" -or
        $transferLow[-1].frame -ne "1869" -or
        $transferLow[-1].value -ne "00" -or
        $transferMiddle[-1].value -ne "00" -or
        $transferHigh[-1].value -ne "00" -or
        $handshake.Count -ne 2 -or
        ($handshake.frame -join ",") -ne "1649,1870" -or
        ($handshake.value -join ",") -ne "01,02"
    ) {
        throw "BS-3 completion award/presentation changed."
    }
}

Write-Output "BS-3 emitter, objective, and result semantics verified."
Write-Output (
    "Emitters: three six-target lanes maintain live counts 2D28-2D2A."
)
Write-Output (
    "Contacts: emitted targets award 100 stored units; eight-hit objectives " +
    "award 100 per hit plus 1,000 on destruction."
)
Write-Output (
    "Completion: objective bytes 2D2F-2D31 select multiplier 20 when clear."
)
if (-not $SkipResults) {
    Write-Output (
        "Timeout: zero progress produces 240 glyph ticks and no special award."
    )
    Write-Output (
        "All-clear: multiplier 20 builds 20,000 stored units and transfers " +
        "them in 40 chunks."
    )
}
