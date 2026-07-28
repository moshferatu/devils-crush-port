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
    for ($index = 0; $index -lt $expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $expected[$index]) {
            throw "$Label changed at ROM offset 0x$('{0:X6}' -f ($Offset + $index))."
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

function Assert-Call {
    param(
        [object] $Call,
        [int] $Sequence,
        [int] $Frame,
        [string] $Pc,
        [string] $X,
        [string] $ReturnPc,
        [int64] $Cycles,
        [int] $Writes,
        [string] $Label
    )

    if (
        [int] $Call.sequence -ne $Sequence -or
        [int] $Call.frame -ne $Frame -or
        $Call.physical_bank -ne "05" -or
        $Call.logical_pc -ne $Pc -or
        $Call.input_x -ne $X -or
        $Call.return_instruction_pc -ne $ReturnPc -or
        [int64] $Call.body_master_cycles -ne $Cycles -or
        [int] $Call.write_count -ne $Writes
    ) {
        throw "$Label call changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# BS-6 has one two-objective controller and two indexed objective records.
Assert-RomHex $rom 0x008464 @"
00 0B E3 71 00 00 20 00 00 60 83 00
00 0C F8 71 50 00 30 00 36 88 82 00
00 0D F8 71 C0 00 30 00 36 88 82 01
"@ "BS-6 controller and objective records"

# The controller initializes two remaining objectives. All clear selects
# multiplier $0F (15) and enters the shared completion path.
Assert-RomHex $rom 0x00B1E3 @"
A9 02 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 0F 85 CA
4C DB 44 60
"@ "BS-6 completion controller"

# Both objectives begin with a ten-hit counter and retain their layout index.
Assert-RomHex $rom 0x00B1F8 @"
9D 80 3C A9 80 9D 80 38 A9 0A 9D 00 3A A9 0A 9D 00
3C A9 10 9D 00 38 A9 81 9D 00 3D 20 18 C0 20 81 C0
29 3F 18 69 40 9D 00 3B 9E 80 39 20 18 C0
"@ "BS-6 objective initializer"

# Each hit awards 50 stored units and command $2E. The tenth returns carry
# to the caller, which adds 2,000, decrements the shared count, emits the
# destruction effect, and releases the actor.
Assert-RomHex $rom 0x00B2A8 @"
BD 00 38 10 1D 29 7F 9D 00 38 A9 32 85 2F 64 30 20
2A C0 A9 2E 20 87 C0 20 B9 68 DE 00 3C D0 02 38 60
18 60 A9 D0 85 2F A9 07 85 30 20 2A C0 CE 2E 2D 20
F6 65 4C 27 C0
"@ "BS-6 objective hit and destruction"

# The objective animation can emit type-$C2 hazards. Preserve the allocator
# and its RNG-derived cadence as part of the retained event family.
Assert-RomHex $rom 0x00B2E0 @"
20 24 C0 90 01 60 A9 C2 99 00 30 18 BD 80 31 69 28
99 80 31 62 7D 00 32 99 00 32 18 BD 00 33 69 08 99
00 33 62 7D 80 33 99 80 33 A9 3B 99 00 39 A9 84 99
80 30 A9 13 99 00 38 A9 81 99 00 3D A9 08 99 80 38
A9 4E 99 00 3F A9 73 99 80 3F
"@ "BS-6 emitted-hazard allocator"

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
    $calls.Count -ne 9 -or
    $writes.Count -ne 1233 -or
    $states.Count -ne 1130 -or
    (
        -not $SkipResults -and
        (
            $timeoutStates.Count -ne 1156 -or
            $completionStates.Count -ne 2239
        )
    )
) {
    throw "BS-6 event/result artifact populations changed."
}

Assert-Call $calls[0] 1 1342 "71E3" "2C" "C5E9" 213 8 `
    "Controller initialization"
Assert-Call $calls[1] 2 1342 "71F8" "34" "C5E9" 312 13 `
    "First objective initialization"
Assert-Call $calls[2] 3 1342 "71F8" "35" "C5E9" 312 13 `
    "Second objective initialization"
Assert-Call $calls[3] 4 1342 "71EB" "2C" "71F7" 27 0 `
    "Initial controller update"
Assert-Call $calls[4] 5 1404 "71EB" "2C" "71F7" 27 0 `
    "Event controller update"
Assert-Call $calls[5] 6 1404 "72A8" "34" "72C9" 10914 182 `
    "Terminal hit helper"
Assert-Call $calls[6] 7 1404 "7228" "34" "C594" 53598 681 `
    "Terminal objective update"
Assert-Call $calls[7] 8 1404 "72A8" "35" "72CB" 8637 161 `
    "Nonterminal hit helper"
Assert-Call $calls[8] 9 1404 "7228" "35" "726F" 9237 175 `
    "Nonterminal objective update"

Assert-Write $writes 1 "05" "71E5" "0D2E" "02" `
    "Remaining-objective initialization"
Assert-Write $writes 2 "05" "71F8" "1CB4" "00" `
    "First objective index"
Assert-Write $writes 2 "05" "7207" "1C34" "0A" `
    "First objective hit-count initialization"
Assert-Write $writes 3 "05" "71F8" "1CB5" "01" `
    "Second objective index"
Assert-Write $writes 3 "05" "7207" "1C35" "0A" `
    "Second objective hit-count initialization"
Assert-Write $writes 6 "00" "F56B" "0DB7" "32" `
    "Terminal ordinary-hit award"
Assert-Write $writes 6 "00" "ECFD" "002A" "2E" `
    "Terminal hit audio command"
Assert-Write $writes 6 "05" "72C3" "1C34" "00" `
    "Terminal hit counter"
Assert-Write $writes 7 "00" "F56B" "0DB7" "02" `
    "Terminal destruction award"
Assert-Write $writes 7 "05" "72D7" "0D2E" "01" `
    "Remaining-objective decrement"
Assert-Write $writes 8 "00" "F56B" "0DB7" "34" `
    "Nonterminal hit award"
Assert-Write $writes 8 "00" "ECFD" "002A" "2E" `
    "Nonterminal hit audio command"
Assert-Write $writes 8 "05" "72C3" "1C35" "01" `
    "Nonterminal hit counter"

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
    $bonusLow.Count -ne 3 -or
    ($bonusLow.value -join ",") -ne "32,02,34"
) {
    throw "BS-6 objective score timeline changed."
}

if (-not $SkipResults) {
    $timeoutTicks = @(Select-StateWrites $timeoutStates "2D55" "4DC4")
    $timeoutAdvances = @(Select-StateWrites $timeoutStates "2D67" "4DD7")
    if (
        $timeoutTicks.Count -ne 240 -or
        $timeoutAdvances.Count -ne 60 -or
        $timeoutAdvances[0].frame -ne "1530" -or
        $timeoutAdvances[0].value -ne "01" -or
        $timeoutAdvances[-1].frame -ne "1766" -or
        $timeoutAdvances[-1].value -ne "3C" -or
        @(Select-StateWrites $timeoutStates "2DB7" "4A79").Count -ne 0 -or
        @(Select-StateWrites $timeoutStates "2DB7" "46DD").Count -ne 0
    ) {
        throw "BS-6 timeout presentation changed."
    }

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
        $completionTicks.Count -ne 392 -or
        $completionAdvances.Count -ne 98 -or
        $completionAdvances[0].frame -ne "1530" -or
        $completionAdvances[0].value -ne "01" -or
        $completionAdvances[-1].frame -ne "1918" -or
        $completionAdvances[-1].value -ne "62" -or
        $awardLow.Count -ne 15 -or
        $awardMiddle.Count -ne 15 -or
        $awardHigh.Count -ne 15 -or
        $awardLow[-1].value -ne "98" -or
        $awardMiddle[-1].value -ne "3A" -or
        $awardHigh[-1].value -ne "00" -or
        $transferLow.Count -ne 30 -or
        $transferMiddle.Count -ne 30 -or
        $transferHigh.Count -ne 30 -or
        $transferLow[0].frame -ne "1831" -or
        $transferLow[0].value -ne "A4" -or
        $transferMiddle[0].value -ne "38" -or
        $transferHigh[0].value -ne "00" -or
        $transferLow[-1].frame -ne "1860" -or
        $transferLow[-1].value -ne "00" -or
        $transferMiddle[-1].value -ne "00" -or
        $transferHigh[-1].value -ne "00" -or
        $handshake.Count -ne 2 -or
        ($handshake.frame -join ",") -ne "1650,1861" -or
        ($handshake.value -join ",") -ne "01,02"
    ) {
        throw "BS-6 completion award/presentation changed."
    }
}

Write-Output "BS-6 objective-pair and result semantics verified."
Write-Output "Objectives: two indexed ten-hit actors share remaining count 2D2E."
Write-Output (
    "Contacts: 50 stored units and command 2E per hit; the tenth adds " +
    "2,000 and destroys the actor."
)
Write-Output "Completion: zero remaining objectives select multiplier 15."
if (-not $SkipResults) {
    Write-Output (
        "Timeout: zero progress produces 240 glyph ticks and no special award."
    )
    Write-Output (
        "All-clear: multiplier 15 builds 15,000 stored units and transfers " +
        "them in 30 chunks."
    )
}
