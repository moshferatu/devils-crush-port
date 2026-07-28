# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $CallPath,

    [Parameter(Mandatory = $true)]
    [string] $WritePath
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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# A collision result in bit 7 of $3800,X enters the score/audio/hit-count
# branch. The same update then decrements the per-target movement timer;
# expiry invokes RNG site 04:4805, refreshes the timer, and calls 04:4836.
Assert-RomHex $rom 0x0087E0 @"
BD 00 38 10 1B 29 7F 9D 00 38 DE 00 3C F0 25 A9
32 85 2F 64 30 20 2A C0 A9 36 20 87 C0 20 B9 68
DE 00 3B D0 0E 20 81 C0 29 2F 18 69 C0 9D 00 3B
20 36 48 60
"@ "BS-1 target contact and expiry branches"

# The expiry helper allocates one type-$C3 actor, copies the source target
# position with a +$18 vertical offset, installs dispatch 04:488D, and emits
# audio command $46.
Assert-RomHex $rom 0x008836 @"
20 24 C0 90 01 60 A9 C3 99 00 30 BD 80 31 99 80
31 BD 00 32 99 00 32 18 BD 00 33 69 18 99 00 33
62 7D 80 33 99 80 33 A9 17 99 00 39 A9 84 99 80
30 A9 03 99 00 38 A9 8D 99 00 3F A9 48 99 80 3F
A9 06 99 80 38 A9 01 99 00 3A DA 02 20 1B C0 02
FA A9 46 20 87 C0 60
"@ "BS-1 timer-expiry effect allocation"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
if ($calls.Count -ne 5 -or $writes.Count -ne 564) {
    throw "BS-1 event artifact populations changed."
}

$expectedSlots = @("34", "35", "50", "51", "52")
$expectedCycles = @(13590, 14016, 891, 1422, 765)
$expectedWrites = @(222, 270, 22, 31, 19)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne 1403 -or
        $call.physical_bank -ne "04" -or
        $call.logical_pc -ne "47D5" -or
        $call.input_x -ne $expectedSlots[$index] -or
        $call.output_x -ne $expectedSlots[$index] -or
        $call.return_instruction_pc -ne "4813" -or
        [int64] $call.body_master_cycles -ne $expectedCycles[$index] -or
        [int] $call.write_count -ne $expectedWrites[$index]
    ) {
        throw "BS-1 event call $($index + 1) changed."
    }
}
if (
    $calls[0].output_a -ne "68" -or $calls[0].output_y -ne "62" -or
    $calls[1].output_a -ne "04" -or $calls[1].output_y -ne "63"
) {
    throw "BS-1 event allocation outputs changed."
}

# Slot $34 takes one genuine nonterminal contact: collision bit cleared,
# hit count 6->5, 50 stored units submitted, command $36 accepted, and its
# ordinary timer decremented from $AD to $AC.
Assert-Write $writes 1 "04" "47E7" "1834" "10" `
    "Target collision clear"
Assert-Write $writes 1 "04" "47EA" "1C34" "05" `
    "Target hit-count decrement"
Assert-Write $writes 1 "04" "47F1" "002F" "32" `
    "Target score input"
Assert-Write $writes 1 "00" "F56B" "0DB7" "32" `
    "Target bonus low byte"
Assert-Write $writes 1 "00" "F573" "0DB8" "00" `
    "Target bonus middle byte"
Assert-Write $writes 1 "00" "F57A" "0DB9" "00" `
    "Target bonus high byte"
Assert-Write $writes 1 "00" "ECFD" "002A" "36" `
    "Target audio command"
Assert-Write $writes 1 "04" "4800" "1B34" "AC" `
    "Target ordinary timer decrement"

# Slot $35 independently reaches zero, consumes RNG at 04:4805, refreshes
# $3B35 to $EB, and allocates the expiry effect after the hit impact already
# claimed dynamic slot $62. The effect therefore occupies slot $63 here.
Assert-Write $writes 2 "04" "4800" "1B35" "00" `
    "Target timer expiry"
Assert-Write $writes 2 "04" "480D" "1B35" "EB" `
    "Target timer refresh"
Assert-Write $writes 2 "04" "483E" "1063" "C3" `
    "Expiry effect dispatch type"
Assert-Write $writes 2 "04" "4844" "11E3" "78" `
    "Expiry effect horizontal position"
Assert-Write $writes 2 "04" "484A" "1263" "00" `
    "Expiry effect horizontal fraction"
Assert-Write $writes 2 "04" "4853" "1363" "5C" `
    "Expiry effect vertical position"
Assert-Write $writes 2 "04" "485A" "13E3" "00" `
    "Expiry effect vertical fraction"
Assert-Write $writes 2 "04" "485F" "1963" "17" `
    "Expiry effect animation"
Assert-Write $writes 2 "04" "4864" "10E3" "84" `
    "Expiry effect type"
Assert-Write $writes 2 "04" "4869" "1863" "03" `
    "Expiry effect collision selector"
Assert-Write $writes 2 "04" "486E" "1F63" "8D" `
    "Expiry effect dispatch low byte"
Assert-Write $writes 2 "04" "4873" "1FE3" "48" `
    "Expiry effect dispatch high byte"
Assert-Write $writes 2 "04" "4878" "18E3" "06" `
    "Expiry effect completion selector"
Assert-Write $writes 2 "04" "487D" "1A63" "01" `
    "Expiry effect lifetime"
Assert-Write $writes 2 "00" "ECFD" "002A" "46" `
    "Expiry effect audio command"

$unexpectedExpiryHit = @(
    $writes |
        Where-Object {
            [int] $_.call_sequence -eq 2 -and
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -in @("47E7", "47EA", "47F1")
        }
)
if ($unexpectedExpiryHit.Count -ne 0) {
    throw "Timer-expiry target unexpectedly took the contact branch."
}

Write-Output "BS-1 contact and timer-expiry semantics verified."
Write-Output (
    "Contact route: slot 34 clears collision, decrements 6->5, awards 50 " +
    "stored units, and emits command 36."
)
Write-Output (
    "Expiry route: slot 35 refreshes timer 00->EB through RNG site 04:4805."
)
Write-Output (
    "Expiry effect: type C3 actor allocated at slot 63 with dispatch " +
    "04:488D and command 46."
)
