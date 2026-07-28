# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $ObjectiveCallPath,

    [Parameter(Mandatory = $true)]
    [string] $ObjectiveWritePath,

    [Parameter(Mandatory = $true)]
    [string] $ObjectiveStatePath,

    [Parameter(Mandatory = $true)]
    [string] $EmitterCallPath,

    [Parameter(Mandatory = $true)]
    [string] $EmitterWritePath,

    [Parameter(Mandatory = $true)]
    [string] $TargetCallPath,

    [Parameter(Mandatory = $true)]
    [string] $TargetWritePath,

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

# BS-4 interleaves three indexed eight-hit objectives with three paired
# source actors. The final record byte is the shared objective index.
Assert-RomHex $rom 0x008252 @"
00 0B 10 6A 00 00 20 00 00 60 83 00
00 0C 31 6A 50 00 28 00 2D 88 82 00
00 0D 7F 6A 50 00 38 00 30 C0 82 00
"@ "BS-4 controller and first objective/source pair"
Assert-RomHex $rom 0x00828E @"
00 10 31 6A 88 00 38 00 2D 88 82 01
00 11 7F 6A 88 00 48 00 30 C0 82 01
"@ "BS-4 second objective/source pair"
Assert-RomHex $rom 0x0082BE @"
00 14 31 6A C0 00 28 00 2D 88 82 02
00 15 7F 6A C0 00 38 00 30 C0 82 02
"@ "BS-4 third objective/source pair"

# The controller owns the same three objective bytes as BS-3. All clear
# selects multiplier $14 (20) and enters the shared result path.
Assert-RomHex $rom 0x00AA10 @"
A9 01 8D 2F 2D 8D 30 2D 8D 31 2D 20 18 C0 AD 2F
2D 0D 30 2D 0D 31 2D D0 07 A9 14 85 CA 4C DB 44
60
"@ "BS-4 completion controller"

# Each objective takes eight hits. Ordinary hits award 100 stored units and
# command $2E. The eighth hit clears the indexed objective byte and adds a
# 1,000-unit destruction award.
Assert-RomHex $rom 0x00AA31 @"
9D 00 3E A9 08 9D 00 3C A9 10 9D 00 38 A9 81 9D 00
3D 20 18 C0 BD 00 38 10 1B 29 7F 9D 00 38 A9 64 85
2F 64 30 20 2A C0 A9 2E 20 87 C0 DE 00 3C F0 04 20
B9 68 60 BC 00 3E 62 99 2F 2D 20 F6 65 A9 E8 85 2F
A9 03 85 30 20 2A C0 4C 27 C0
"@ "BS-4 objective hit and destruction"

# The paired source continuation allocates a linked type-$C2 target. It
# copies the objective index and position, installs dispatch $6B7E, and
# marks the source as owning a live target.
Assert-RomHex $rom 0x00AACC @"
20 16 6B A9 04 20 21 C0 20 18 C0 38 BD 00 33 E9 08
9D 00 33 BD 80 33 E9 00 9D 80 33 4C 8D 6A
"@ "BS-4 paired source allocation continuation"
Assert-RomHex $rom 0x00AB16 @"
20 24 C0 90 01 60 A9 C2 99 00 30 18 BD 80 31 69 10
99 80 31 62 7D 00 32 99 00 32 18 BD 00 33 69 10 99
00 33 62 7D 80 33 99 80 33 BD 00 3E 99 00 3E A9 03
99 00 38 A9 2F 99 00 39 A9 84 99 80 30 A9 01 99 00
3B A9 01 99 00 3A A9 01 99 80 38 A9 7E 99 00 3F A9
6B 99 80 3F A9 FF 99 80 3C 8A 99 80 3E A9 FF 9D 80
3C 60
"@ "BS-4 linked target allocator"

# A main target hit awards 100 stored units and response $3B before yielding
# through the animation continuation. The subsequent ROM continuation emits
# four orbiting children at $6D27; a child contact also awards 100 and frees
# that child.
Assert-RomHex $rom 0x00AC06 @"
A9 64 85 2F 64 30 20 2A C0 A9 3B 8D 6E 2D 20 B1 66
A9 04 9D 80 39 A9 06 20 21 C0 20 18 C0
"@ "BS-4 linked target contact"
Assert-RomHex $rom 0x00AC24 @"
A9 04 85 26 20 24 C0 B0 6F BD 80 31 85 35 BD 00 32
85 36 BD 00 33 85 37 BD 80 33 85 38
"@ "BS-4 four-child allocation continuation"
Assert-RomHex $rom 0x00AD27 @"
BC 80 3E B9 80 3C F0 50 BD 00 38 30 4B FE 00 3E BD
00 3E 29 1F 9D 00 3E
"@ "BS-4 orbiting child update"
Assert-RomHex $rom 0x00AD7F @"
A9 64 85 2F 64 30 20 2A C0 20 9E 66 4C 27 C0
"@ "BS-4 orbiting child contact"

# The shared result builder consumes the controller multiplier, adding 1,000
# stored units once per count.
Assert-RomHex $rom 0x008A71 @"
A4 CA 18 AD B7 2D 69 E8 8D B7 2D AD B8 2D 69 03
8D B8 2D 62 6D B9 2D 8D B9 2D 88 D0 E5 60
"@ "Bonus special-award builder"

$objectiveCalls = @(Import-TraceCsv $ObjectiveCallPath)
$objectiveWrites = @(Import-TraceCsv $ObjectiveWritePath)
$objectiveStates = @(Import-TraceCsv $ObjectiveStatePath)
$emitterCalls = @(Import-TraceCsv $EmitterCallPath)
$emitterWrites = @(Import-TraceCsv $EmitterWritePath)
$targetCalls = @(Import-TraceCsv $TargetCallPath)
$targetWrites = @(Import-TraceCsv $TargetWritePath)
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
    $objectiveCalls.Count -ne 12 -or
    $objectiveWrites.Count -ne 870 -or
    $objectiveStates.Count -ne 1114 -or
    $emitterCalls.Count -ne 3 -or
    $emitterWrites.Count -ne 288 -or
    $targetCalls.Count -ne 1 -or
    $targetWrites.Count -ne 76 -or
    (
        -not $SkipResults -and
        (
            $timeoutStates.Count -ne 1158 -or
            $completionStates.Count -ne 2328
        )
    )
) {
    throw "BS-4 event/result artifact populations changed."
}

$expectedObjectiveCalls = @(
    @("6A10", "2C", "C5E9", 243, 10),
    @("6A31", "34", "C5E9", 270, 11),
    @("6A31", "52", "C5E9", 270, 11),
    @("6A31", "56", "C5E9", 270, 11),
    @("6A1E", "2C", "6A30", 57, 0),
    @("6A46", "34", "6A66", 27, 0),
    @("6A46", "52", "6A66", 27, 0),
    @("6A46", "56", "6A66", 27, 0),
    @("6A1E", "2C", "6A30", 57, 0),
    @("6A46", "34", "C594", 54780, 666),
    @("6A46", "52", "6A66", 9930, 161),
    @("6A46", "56", "6A66", 27, 0)
)
for ($index = 0; $index -lt $objectiveCalls.Count; ++$index) {
    $call = $objectiveCalls[$index]
    $expected = $expectedObjectiveCalls[$index]
    $expectedFrame = if ($index -lt 8) { 1342 } else { 1404 }
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne $expectedFrame -or
        $call.physical_bank -ne "05" -or
        $call.logical_pc -ne $expected[0] -or
        $call.input_x -ne $expected[1] -or
        $call.return_instruction_pc -ne $expected[2] -or
        [int64] $call.body_master_cycles -ne $expected[3] -or
        [int] $call.write_count -ne $expected[4]
    ) {
        throw "BS-4 objective call $($index + 1) changed."
    }
}

$expectedEmitterCalls = @(
    @("6B16", "35", "6B7D", 6363, 46),
    @("6ACC", "35", "C5E9", 6747, 61),
    @("6B7E", "66", "6C05", 6369, 181)
)
for ($index = 0; $index -lt $emitterCalls.Count; ++$index) {
    $call = $emitterCalls[$index]
    $expected = $expectedEmitterCalls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne 1404 -or
        $call.physical_bank -ne "05" -or
        $call.logical_pc -ne $expected[0] -or
        $call.input_x -ne $expected[1] -or
        $call.return_instruction_pc -ne $expected[2] -or
        [int64] $call.body_master_cycles -ne $expected[3] -or
        [int] $call.write_count -ne $expected[4]
    ) {
        throw "BS-4 emitter call $($index + 1) changed."
    }
}

$targetCall = $targetCalls[0]
if (
    [int] $targetCall.sequence -ne 1 -or
    [int] $targetCall.frame -ne 1404 -or
    $targetCall.physical_bank -ne "05" -or
    $targetCall.logical_pc -ne "6C06" -or
    $targetCall.input_x -ne "66" -or
    $targetCall.return_instruction_pc -ne "C5E9" -or
    [int64] $targetCall.body_master_cycles -ne 7311 -or
    [int] $targetCall.write_count -ne 76
) {
    throw "BS-4 linked-target contact call changed."
}

# Objective initialization and the two selected hit branches.
Assert-Write $objectiveWrites 1 "05" "6A12" "0D2F" "01" `
    "First objective flag initialization"
Assert-Write $objectiveWrites 1 "05" "6A15" "0D30" "01" `
    "Second objective flag initialization"
Assert-Write $objectiveWrites 1 "05" "6A18" "0D31" "01" `
    "Third objective flag initialization"
Assert-Write $objectiveWrites 2 "05" "6A36" "1C34" "08" `
    "First objective hit-count initialization"
Assert-Write $objectiveWrites 10 "00" "F56B" "0DB7" "64" `
    "Terminal objective ordinary-hit award"
Assert-Write $objectiveWrites 10 "00" "ECFD" "002A" "2E" `
    "Terminal objective audio command"
Assert-Write $objectiveWrites 10 "05" "6A5E" "1C34" "00" `
    "Terminal objective counter"
Assert-Write $objectiveWrites 10 "05" "6A6B" "0D2F" "00" `
    "Terminal objective flag clear"
Assert-Write $objectiveWrites 10 "00" "F56B" "0DB7" "4C" `
    "Terminal objective destruction award"
Assert-Write $objectiveWrites 11 "00" "F56B" "0DB7" "B0" `
    "Nonterminal objective award"
Assert-Write $objectiveWrites 11 "00" "ECFD" "002A" "2E" `
    "Nonterminal objective audio command"
Assert-Write $objectiveWrites 11 "05" "6A5E" "1C52" "01" `
    "Nonterminal objective counter"

# The source creates slot $66, links it back to source $35 and objective 0,
# and installs the target update entry before the immediate refresh.
Assert-Write $emitterWrites 1 "05" "6B1E" "1066" "C2" `
    "Linked target type"
Assert-Write $emitterWrites 1 "05" "6B27" "11E6" "60" `
    "Linked target horizontal position"
Assert-Write $emitterWrites 1 "05" "6B37" "1366" "48" `
    "Linked target vertical position"
Assert-Write $emitterWrites 1 "05" "6B44" "1E66" "00" `
    "Linked target objective index"
Assert-Write $emitterWrites 1 "05" "6B67" "1F66" "7E" `
    "Linked target dispatch low byte"
Assert-Write $emitterWrites 1 "05" "6B6C" "1FE6" "6B" `
    "Linked target dispatch high byte"
Assert-Write $emitterWrites 1 "05" "6B75" "1EE6" "35" `
    "Linked target source slot"
Assert-Write $emitterWrites 1 "05" "6B7A" "1CB5" "FF" `
    "Source live-target marker"

# The independent contact route proves the live target's score/response and
# animation continuation without conflating it with allocation timing.
Assert-Write $targetWrites 1 "00" "F56B" "0DB7" "64" `
    "Linked target contact award"
Assert-Write $targetWrites 1 "05" "6C11" "0D6E" "3B" `
    "Linked target impact selector"
Assert-Write $targetWrites 1 "05" "6C19" "19E6" "04" `
    "Linked target hit appearance"
Assert-Write $targetWrites 1 "01" "C5FB" "1F66" "21" `
    "Linked target continuation low byte"
Assert-Write $targetWrites 1 "01" "C600" "1FE6" "6C" `
    "Linked target continuation high byte"

$bonusLow = @(
    $objectiveStates |
        Where-Object {
            $_.logical_address -eq "2DB7" -and
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "F56B" -and
            $_.frame -eq "1404"
        }
)
if (
    $bonusLow.Count -ne 3 -or
    ($bonusLow.value -join ",") -ne "64,4C,B0"
) {
    throw "BS-4 objective score timeline changed."
}

if (-not $SkipResults) {
    # The unmodified stage reaches the shared zero-progress timeout.
    $timeoutTicks = @(Select-StateWrites $timeoutStates "2D55" "4DC4")
    $timeoutAdvances = @(Select-StateWrites $timeoutStates "2D67" "4DD7")
    if (
        $timeoutTicks.Count -ne 240 -or
        $timeoutAdvances.Count -ne 60 -or
        $timeoutAdvances[0].frame -ne "1528" -or
        $timeoutAdvances[0].value -ne "01" -or
        $timeoutAdvances[-1].frame -ne "1764" -or
        $timeoutAdvances[-1].value -ne "3C" -or
        @(Select-StateWrites $timeoutStates "2DB7" "4A79").Count -ne 0 -or
        @(Select-StateWrites $timeoutStates "2DB7" "46DD").Count -ne 0
    ) {
        throw "BS-4 timeout presentation changed."
    }

    # All clear selects multiplier 20, builds 20,000 stored units, and
    # transfers the result in 40 chunks of 500.
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
        $completionAdvances[0].frame -ne "1528" -or
        $completionAdvances[0].value -ne "01" -or
        $completionAdvances[-1].frame -ne "1928" -or
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
        $transferLow[0].frame -ne "1829" -or
        $transferLow[0].value -ne "2C" -or
        $transferMiddle[0].value -ne "4C" -or
        $transferHigh[0].value -ne "00" -or
        $transferLow[-1].frame -ne "1868" -or
        $transferLow[-1].value -ne "00" -or
        $transferMiddle[-1].value -ne "00" -or
        $transferHigh[-1].value -ne "00" -or
        $handshake.Count -ne 2 -or
        ($handshake.frame -join ",") -ne "1648,1869" -or
        ($handshake.value -join ",") -ne "01,02"
    ) {
        throw "BS-4 completion award/presentation changed."
    }
}

Write-Output "BS-4 objective, linked-target, and result semantics verified."
Write-Output (
    "Objectives: three eight-hit records use flags 2D2F-2D31 and command 2E."
)
Write-Output (
    "Linked targets: paired sources allocate type C2 actors; contacts award " +
    "100 stored units and lead to four orbiting children."
)
Write-Output (
    "Completion: clearing all three objective bytes selects multiplier 20."
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
