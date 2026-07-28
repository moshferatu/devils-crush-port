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

# The BS-5 controller record is followed by three support actors and a
# five-by-three field of 15 identical objective initializers.
Assert-RomHex $rom 0x008343 @"
00 0B 0E 6E 00 00 20 00 00 60 83 00
"@ "BS-5 controller record"
$expectedX = @(0x58, 0x78, 0x98, 0xB8, 0xD8)
$expectedY = @(0x40, 0x50, 0x60)
for ($row = 0; $row -lt 3; ++$row) {
    for ($column = 0; $column -lt 5; ++$column) {
        $record = 5 + $row * 5 + $column
        $offset = 0x008337 + $record * 12
        $expected = @(
            0x00, (0x0A + $record), 0x00, 0x71,
            $expectedX[$column], 0x00, $expectedY[$row], 0x00,
            0x34, 0x80, 0x82, 0x00
        )
        for ($byte = 0; $byte -lt 12; ++$byte) {
            if ($rom[$offset + $byte] -ne $expected[$byte]) {
                throw "BS-5 objective layout record $record changed."
            }
        }
    }
}

# The controller initializes the remaining-target count to 15. When it
# reaches zero, multiplier $1E (30) enters the shared completion path.
Assert-RomHex $rom 0x00AE0E @"
A9 0F 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 1E 85 CA
4C DB 44 60
"@ "BS-5 completion controller"

# Each objective starts with four animation/collision-state units. A contact
# awards 100 and command $21. The third visible-state contact adds 400,
# decrements the shared count, and converts the actor to its completed form.
Assert-RomHex $rom 0x00B100 @"
A9 10 9D 00 38 A9 84 9D 00 3D A9 04 9D 00 3C 20 18
C0 BD 00 38 10 1D 29 7F 9D 00 38 A9 64 85 2F 64 30
20 2A C0 A9 21 20 87 C0 FE 80 39 BD 80 39 C9 03 F0
01 60 A9 90 85 2F A9 01 85 30 20 2A C0 CE 2E 2D 20
52 66 BD 80 30 09 40 9D 80 30 20 18 C0 AD 2E 2D F0
01 60 20 9E 66 4C 27 C0
"@ "BS-5 three-contact objective"

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
    $calls.Count -ne 52 -or
    $writes.Count -ne 725 -or
    $states.Count -ne 1101 -or
    (
        -not $SkipResults -and
        (
            $timeoutStates.Count -ne 1156 -or
            $completionStates.Count -ne 2483
        )
    )
) {
    throw "BS-5 event/result artifact populations changed."
}

Assert-Call $calls[0] 1 1342 "6E0E" "2C" "C5E9" 213 8 `
    "Controller initialization"
for ($index = 1; $index -le 3; ++$index) {
    $slot = @("34", "35", "50")[$index - 1]
    Assert-Call $calls[$index] ($index + 1) 1342 "6E23" $slot `
        "C5E9" 270 11 "Support initialization $index"
}
$targetSlots = @(
    "51", "52", "53", "54", "55", "56", "57", "58",
    "59", "5A", "62", "63", "64", "65", "66"
)
for ($index = 0; $index -lt $targetSlots.Count; ++$index) {
    Assert-Call $calls[4 + $index] (5 + $index) 1342 "7100" `
        $targetSlots[$index] "C5E9" 255 10 `
        "Objective initialization $($index + 1)"
}
Assert-Call $calls[19] 20 1342 "715D" "69" "C5E9" 228 9 `
    "Presentation initialization"
Assert-Call $calls[20] 21 1342 "6E16" "2C" "6E22" 27 0 `
    "Initial controller update"
for ($index = 0; $index -lt $targetSlots.Count; ++$index) {
    Assert-Call $calls[21 + $index] (22 + $index) 1342 "7112" `
        $targetSlots[$index] "7134" 27 0 `
        "Initial objective update $($index + 1)"
}
Assert-Call $calls[36] 37 1404 "6E16" "2C" "6E22" 27 0 `
    "Event controller update"
Assert-Call $calls[37] 38 1404 "7112" "51" "C5E9" 30006 377 `
    "Terminal objective contact"
Assert-Call $calls[38] 39 1404 "7112" "52" "7134" 6093 148 `
    "Nonterminal objective contact"
for ($index = 2; $index -lt $targetSlots.Count; ++$index) {
    Assert-Call $calls[37 + $index] (38 + $index) 1404 "7112" `
        $targetSlots[$index] "7134" 27 0 `
        "Remaining objective update $($index + 1)"
}

Assert-Write $writes 1 "05" "6E10" "0D2E" "0F" `
    "Remaining-target initialization"
Assert-Write $writes 5 "05" "7102" "1851" "10" `
    "Objective collision initialization"
Assert-Write $writes 5 "05" "710C" "1C51" "04" `
    "Objective state initialization"
Assert-Write $writes 38 "00" "F56B" "0DB7" "64" `
    "Terminal contact ordinary award"
Assert-Write $writes 38 "00" "ECFD" "002A" "21" `
    "Terminal contact audio command"
Assert-Write $writes 38 "05" "712A" "19D1" "03" `
    "Terminal visible state"
Assert-Write $writes 38 "00" "F56B" "0DB7" "F4" `
    "Terminal contact completion award"
Assert-Write $writes 38 "05" "7140" "0D2E" "0E" `
    "Remaining-target decrement"
Assert-Write $writes 39 "00" "F56B" "0DB7" "58" `
    "Nonterminal contact award"
Assert-Write $writes 39 "00" "ECFD" "002A" "21" `
    "Nonterminal contact audio command"
Assert-Write $writes 39 "05" "712A" "19D2" "02" `
    "Nonterminal visible state"

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
    ($bonusLow.value -join ",") -ne "64,F4,58"
) {
    throw "BS-5 objective score timeline changed."
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
        throw "BS-5 timeout presentation changed."
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
        $completionTicks.Count -ne 422 -or
        $completionAdvances.Count -ne 106 -or
        $completionAdvances[0].frame -ne "1530" -or
        $completionAdvances[0].value -ne "01" -or
        $completionAdvances[-1].frame -ne "1950" -or
        $completionAdvances[-1].value -ne "6A" -or
        $awardLow.Count -ne 30 -or
        $awardMiddle.Count -ne 30 -or
        $awardHigh.Count -ne 30 -or
        $awardLow[-1].value -ne "30" -or
        $awardMiddle[-1].value -ne "75" -or
        $awardHigh[-1].value -ne "00" -or
        $transferLow.Count -ne 60 -or
        $transferMiddle.Count -ne 60 -or
        $transferHigh.Count -ne 60 -or
        $transferLow[0].frame -ne "1831" -or
        $transferLow[0].value -ne "3C" -or
        $transferMiddle[0].value -ne "73" -or
        $transferHigh[0].value -ne "00" -or
        $transferLow[-1].frame -ne "1890" -or
        $transferLow[-1].value -ne "00" -or
        $transferMiddle[-1].value -ne "00" -or
        $transferHigh[-1].value -ne "00" -or
        $handshake.Count -ne 2 -or
        ($handshake.frame -join ",") -ne "1650,1891" -or
        ($handshake.value -join ",") -ne "01,02"
    ) {
        throw "BS-5 completion award/presentation changed."
    }
}

Write-Output "BS-5 objective-field and result semantics verified."
Write-Output (
    "Objectives: a five-by-three field contains 15 three-contact targets."
)
Write-Output (
    "Contacts: 100 stored units and command 21 per hit; the third hit adds " +
    "400 and decrements 2D2E."
)
Write-Output (
    "Completion: zero remaining targets select multiplier 30."
)
if (-not $SkipResults) {
    Write-Output (
        "Timeout: zero progress produces 240 glyph ticks and no special award."
    )
    Write-Output (
        "All-clear: multiplier 30 builds 30,000 stored units and transfers " +
        "them in 60 chunks."
    )
}
