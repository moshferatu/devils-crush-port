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

# The second emitter is the first emitter's mirrored subtype-1 partner. It
# starts at x=$3C, advances a four-entry presentation table, owns live count
# $2D27, and shares allocator 04:4FBA.
Assert-RomHex $rom 0x008F2A @"
A9 01 9D 80 34 A9 FF 9D 80 3E A9 3C 9D 80 31 9E 00
32 A9 30 9D 00 33 9E 80 33 A9 84 9D 00 3D 20 18 C0
FE 80 3E BC 80 3E B9 B6 4F C9 FF D0 07 9E 80 3E C2
B9 B6 4F 9D 00 3E A9 40 20 21 C0
"@ "BS-2 second-emitter initializer"
Assert-RomHex $rom 0x008F73 @"
9C 27 2D A9 01 9D 00 3A A9 05 9D 00 3B 20 18 C0
DE 00 3A D0 0D A9 30 9D 00 3A 20 BA 4F DE 00 3B
F0 01 60
"@ "BS-2 second-emitter batch"

# Slot $50 stages the bonus ball at (63,205), installs continuation 04:5166,
# and puts the ball in presentation state 6. Slot $51 installs the repeatable
# collision/capture state 04:5192 with a 10-unit award.
Assert-RomHex $rom 0x00911D @"
AD 6C 2D F0 03 4C 27 C0 A9 01 8D 80 3C A9 1A 9D 80
3C A9 79 9D 00 3E A9 51 9D 80 3E A9 04 9D 00 3A
BD 80 31 8D 80 31 BD 00 32 8D 00 32 BD 00 33 8D
00 33 BD 80 33 8D 80 33 A9 06 8D 80 39 AD 80 30
09 20 8D 80 30 20 18 C0
"@ "BS-2 ball-entry actor"
Assert-RomHex $rom 0x009169 @"
AD 6C 2D F0 03 4C 27 C0 A9 16 9D 80 3C 4C 79 51
A9 01 9D 00 3A A9 79 9D 00 3E A9 51 9D 80 3E A9
92 9D 00 3F A9 51 9D 80 3F BD 00 38 30 01 60 A9
0A 85 2F 64 30 20 2A C0
"@ "BS-2 capture actor"

# Target coordinates map to an 8x5 logical board. Two interior rows retain
# only their end cells, producing 28 valid cells. A valid contact resolves a
# four-tile block, then decrements the shared objective exactly once.
Assert-RomHex $rom 0x009260 @"
38 BD 80 31 E9 60 30 25 4A 4A 4A 4A C9 08 B0 1D
85 26 18 BD 00 33 69 14 38 E9 50 30 10 29 F0 C9
50 B0 0A 4A 18 65 26 A8 B9 4C 53 10 01 60
"@ "BS-2 target-to-board mapping"
Assert-RomHex $rom 0x0092B7 @"
20 ED 52 B0 30 18 A5 2D 69 01 85 2D 62 65 2E 85
2E 20 ED 52 18 A5 2D 69 3F 85 2D 62 65 2E 85 2E
20 ED 52 18 A5 2D 69 01 85 2D 62 65 2E 85 2E 20
ED 52 CE 2E 2D 60
"@ "BS-2 four-tile resolution"
Assert-RomHex $rom 0x00934C @"
00 01 02 03 04 05 06 07
08 FF FF FF FF FF FF 09
0A 0B 0C 0D 0E 0F 10 11
12 FF FF FF FF FF FF 13
14 15 16 17 18 19 1A 1B
"@ "BS-2 28-cell board"
Assert-RomHex $rom 0x00933F @"
A9 64 85 2F 64 30 20 2A C0 18 60 38 60
"@ "BS-2 resolved-tile award"

# Zero accumulated bonus selects timeout/failure. A nonzero bonus plus the
# completion latch reaches the special-points branch. BS-2's controller puts
# $32 in zero-page $CA, so the shared builder adds 1,000 exactly 50 times.
Assert-RomHex $rom 0x008E97 @"
AD 2E 2D D0 07 A9 32 85 CA 4C DB 44 60
"@ "BS-2 completion controller"
Assert-RomHex $rom 0x00859E @"
AD B7 2D 0D B8 2D 0D B9 2D D0 47
"@ "Bonus timeout selector"
Assert-RomHex $rom 0x008648 @"
F0 E7 AD BA 2D F0 55
"@ "Bonus completion selector"
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
    $calls.Count -ne 16 -or
    $writes.Count -ne 3741 -or
    $states.Count -ne 1101 -or
    (
        -not $SkipResults -and
        (
            $timeoutStates.Count -ne 1156 -or
            $completionStates.Count -ne 2800
        )
    )
) {
    throw "BS-2 resolution/result artifact populations changed."
}

$expectedCalls = @(
    @("4EA4", "34", "C5E9", 321, 14),
    @("4F2A", "35", "C5E9", 327, 14),
    @("511D", "50", "C5E9", 501, 18),
    @("5169", "51", "5197", 186, 6),
    @("4FBA", "34", "501E", 5559, 45),
    @("4EFB", "34", "4F0D", 5676, 50),
    @("4FBA", "35", "501E", 5604, 45),
    @("4F83", "35", "4F95", 5721, 50),
    @("52ED", "53", "5349", 789, 17),
    @("52ED", "53", "5349", 1536, 31),
    @("52ED", "53", "5349", 789, 17),
    @("52ED", "53", "5349", 789, 17),
    @("5260", "53", "52EC", 4737, 106),
    @("5079", "53", "C594", 192693, 3299),
    @("5051", "54", "C5E9", 171, 6),
    @("5051", "55", "C5E9", 171, 6)
)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $expected = $expectedCalls[$index]
    $expectedFrame = if ($index -lt 4) { 1342 } else { 1404 }
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne $expectedFrame -or
        $call.physical_bank -ne "04" -or
        $call.logical_pc -ne $expected[0] -or
        $call.input_x -ne $expected[1] -or
        $call.output_x -ne $expected[1] -or
        $call.return_instruction_pc -ne $expected[2] -or
        [int64] $call.body_master_cycles -ne $expected[3] -or
        [int] $call.write_count -ne $expected[4]
    ) {
        throw "BS-2 resolution call $($index + 1) changed."
    }
}

# The paired emitters create subtype 0 at slot $54 and subtype 1 at slot
# $55, with independent live counters and identical five-target batches.
Assert-Write $writes 5 "04" "4FC2" "1054" "C2" `
    "First emitted target"
Assert-Write $writes 5 "04" "501A" "0D26" "01" `
    "First lane live count"
Assert-Write $writes 6 "04" "4F08" "1B34" "04" `
    "First emitter batch decrement"
Assert-Write $writes 7 "04" "4FC2" "1055" "C2" `
    "Second emitted target"
Assert-Write $writes 7 "04" "4FC8" "11D5" "3C" `
    "Second emitted target position"
Assert-Write $writes 7 "04" "501A" "0D27" "01" `
    "Second lane live count"
Assert-Write $writes 8 "04" "4F90" "1B35" "04" `
    "Second emitter batch decrement"

# The remaining stage-specific layout actors install the ball-entry
# continuation and the repeatable 10-unit capture dispatch.
Assert-Write $writes 3 "04" "5141" "1180" "3F" `
    "Ball-entry horizontal placement"
Assert-Write $writes 3 "04" "514D" "1300" "CD" `
    "Ball-entry vertical placement"
Assert-Write $writes 3 "04" "5158" "1980" "06" `
    "Ball-entry presentation state"
Assert-Write $writes 3 "01" "C5E1" "1F50" "66" `
    "Ball-entry continuation"
Assert-Write $writes 4 "04" "5173" "1CD1" "16" `
    "Capture actor state"
Assert-Write $writes 4 "04" "518A" "1F51" "92" `
    "Capture actor dispatch"

# A contact at logical board cell (0,0) clears four tiles. Each tile awards
# 100 stored units, the contact itself awards 50, and only the outer
# resolution decrements the 28-count objective.
Assert-Write $writes 13 "04" "52E9" "0D2E" "1B" `
    "Board objective decrement"
Assert-Write $writes 14 "04" "509B" "002F" "32" `
    "Target contact award"
Assert-Write $writes 14 "04" "50B4" "0D26" "00" `
    "Contact lane-count balance"

$objective = @($states | Where-Object { $_.logical_address -eq "2D2E" })
$bonusLow = @(
    $states |
        Where-Object {
            $_.logical_address -eq "2DB7" -and
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "F56B" -and
            $_.frame -eq "1404"
        }
)
if (
    $objective.Count -ne 3 -or
    ($objective.frame -join ",") -ne "1,1342,1404" -or
    ($objective.value -join ",") -ne "00,1C,1B" -or
    $objective[-1].instruction_logical_pc -ne "52E9" -or
    $bonusLow.Count -ne 5 -or
    ($bonusLow.value -join ",") -ne "32,96,FA,5E,C2"
) {
    throw "BS-2 board-resolution state timeline changed."
}

if (-not $SkipResults) {
    # The unmodified forced-entry stage times out with no accumulated bonus.
    # Its failure presentation advances 60 glyphs over 240 four-tick calls.
    $timeoutTicks = @(Select-StateWrites $timeoutStates "2D55" "4DC4")
    $timeoutResets = @(Select-StateWrites $timeoutStates "2D55" "4DCC")
    $timeoutAdvances = @(Select-StateWrites $timeoutStates "2D67" "4DD7")
    if (
        $timeoutTicks.Count -ne 240 -or
        $timeoutResets.Count -ne 60 -or
        $timeoutAdvances.Count -ne 60 -or
        $timeoutAdvances[0].frame -ne "1529" -or
        $timeoutAdvances[0].value -ne "01" -or
        $timeoutAdvances[-1].frame -ne "1765" -or
        $timeoutAdvances[-1].value -ne "3C" -or
        @(Select-StateWrites $timeoutStates "2DB7" "4A79").Count -ne 0 -or
        @(Select-StateWrites $timeoutStates "2DB7" "46DD").Count -ne 0
    ) {
        throw "BS-2 timeout presentation changed."
    }

    # The targeted completion state retains the real zero-objective
    # controller, which selects multiplier $32. The shared special builder
    # therefore adds 1,000 fifty times and transfers 50,000 in 100 chunks.
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
    $transferMiddle = @(Select-StateWrites $completionStates "2DB8" "46E2")
    $transferHigh = @(Select-StateWrites $completionStates "2DB9" "46E7")
    $handshake = @(Select-StateWrites $completionStates "2D66" "473A")
    if (
        $completionTicks.Count -ne 462 -or
        $completionAdvances.Count -ne 116 -or
        $completionAdvances[0].frame -ne "1529" -or
        $completionAdvances[0].value -ne "01" -or
        $completionAdvances[-1].frame -ne "1989" -or
        $completionAdvances[-1].value -ne "74" -or
        $awardLow.Count -ne 50 -or
        $awardMiddle.Count -ne 50 -or
        $awardHigh.Count -ne 50 -or
        $awardLow[-1].value -ne "50" -or
        $awardMiddle[-1].value -ne "C3" -or
        $awardHigh[-1].value -ne "00" -or
        $transferLow.Count -ne 100 -or
        $transferMiddle.Count -ne 100 -or
        $transferHigh.Count -ne 100 -or
        $transferLow[0].frame -ne "1830" -or
        $transferLow[0].value -ne "5C" -or
        $transferMiddle[0].value -ne "C1" -or
        $transferHigh[0].value -ne "00" -or
        $transferLow[-1].frame -ne "1929" -or
        $transferLow[-1].value -ne "00" -or
        $transferMiddle[-1].value -ne "00" -or
        $transferHigh[-1].value -ne "00" -or
        $handshake.Count -ne 2 -or
        ($handshake.frame -join ",") -ne "1649,1930" -or
        ($handshake.value -join ",") -ne "01,02"
    ) {
        throw "BS-2 completion award/presentation changed."
    }
}

Write-Output "BS-2 movement, resolution, and result semantics verified."
Write-Output (
    "Paired emitters: slots 34/35 create subtype 0/1 targets with " +
    "independent live counters 2D26/2D27."
)
Write-Output (
    "Board resolution: valid 8x5 perimeter cell clears four tiles, awards " +
    "450 stored units total, and decrements 2D2E once."
)
Write-Output (
    "Remaining actors: slot 50 stages the ball at (63,205); slot 51 " +
    "installs the repeatable 10-unit capture state."
)
if (-not $SkipResults) {
    Write-Output (
        "Timeout: zero progress produces 240 glyph ticks and no special award."
    )
    Write-Output (
        "Completion: multiplier 50 builds 50,000 stored units and transfers " +
        "them in 100 chunks."
    )
}
