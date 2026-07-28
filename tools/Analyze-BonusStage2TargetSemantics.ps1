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

# BS-2 has eight layout records. Its first record performs stage setup and
# frees slot $2C; the second record reuses that slot for the objective
# controller. Slots $34/$35 are the paired type-$C2 target emitters.
Assert-RomHex $rom 0x008154 @"
00 0A BE 48 20 00 30 00 0D C8 83 00
00 0B 8F 4E 00 00 20 00 00 60 83 00
00 0C A4 4E 3C 00 30 00 1D E0 82 00
00 0D 2A 4F 3C 00 30 00 3A E0 82 00
00 0E 1D 51 3F 00 CD 00 43 A0 84 00
00 0F 69 51 FC 00 CD 00 43 A0 84 00
00 10 17 68 00 00 20 00 00 E0 80 00
00 11 3C 4A 00 00 20 00 00 60 80 04
FF
"@ "BS-2 eight-record layout"

# The controller initializes $2D2E to $1C (28) and waits for it to reach
# zero before entering the shared completion presentation.
Assert-RomHex $rom 0x008E8F @"
A9 1C 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 32 85 CA
4C DB 44 60
"@ "BS-2 objective controller"

# A source countdown reaching zero reloads to $30, invokes the shared target
# allocator, and decrements its five-emission batch counter.
Assert-RomHex $rom 0x008EFB @"
DE 00 3A D0 0D A9 30 9D 00 3A 20 BA 4F DE 00 3B F0
01 60
"@ "BS-2 first emitter state"
Assert-RomHex $rom 0x008EEB @"
9C 26 2D A9 01 9D 00 3A A9 05 9D 00 3B 20 18 C0
"@ "BS-2 first emitter batch setup"

# The shared allocator creates a type-$C2 target, copies source position and
# subtype, installs one hit, collision selector $10, completion selector
# $81, live dispatch 04:5051, and increments the subtype's live counter at
# $2D26/Y.
Assert-RomHex $rom 0x008FBA @"
20 24 C0 90 01 60 A9 C2 99 00 30 BD 80 31 99 80 31
BD 00 32 99 00 32 BD 00 33 99 00 33 BD 80 33 99 80
33 BD 00 39 99 00 39 BD 00 3E 99 00 37 BD 80 34 99
80 34 A9 10 99 00 38 BD 00 3D 99 00 3D A9 84 99 80
30 A9 10 99 80 3D A9 01 99 00 3C A9 51 99 00 3F
A9 50 99 80 3F BC 80 34 B9 26 2D 1A 99 26 2D 18
60
"@ "BS-2 target allocator"

# Once the target's entrance delay completes, a collision submits 50 stored
# units, selects response $35, runs the common response, decrements its
# subtype live counter, and frees the actor.
Assert-RomHex $rom 0x009079 @"
AD 2E 2D D0 03 4C CD 50 BD 00 37 20 12 5A B0 31 20
D8 50 BD 00 38 10 07 29 7F 9D 00 38 80 01 60 A9 32
85 2F 64 30 20 2A C0 A9 35 8D 6E 2D 20 48 66 20
60 52 BC 80 34 B9 26 2D 3A 99 26 2D 4C 27 C0
"@ "BS-2 target contact state"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 4 -or
    $writes.Count -ne 3269 -or
    $states.Count -ne 1087
) {
    throw "BS-2 target artifact populations changed."
}

$expectedCalls = @(
    [pscustomobject] @{
        Pc = "4FBA"; X = "34"; Return = "501E"; Cycles = 5559; Writes = 45
    },
    [pscustomobject] @{
        Pc = "4EFB"; X = "34"; Return = "4F0D"; Cycles = 5676; Writes = 50
    },
    [pscustomobject] @{
        Pc = "5079"; X = "53"; Return = "C594"; Cycles = 186588; Writes = 3168
    },
    [pscustomobject] @{
        Pc = "5051"; X = "54"; Return = "C5E9"; Cycles = 171; Writes = 6
    }
)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $expected = $expectedCalls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne 1404 -or
        $call.physical_bank -ne "04" -or
        $call.logical_pc -ne $expected.Pc -or
        $call.input_x -ne $expected.X -or
        $call.output_x -ne $expected.X -or
        $call.return_instruction_pc -ne $expected.Return -or
        [int64] $call.body_master_cycles -ne $expected.Cycles -or
        [int] $call.write_count -ne $expected.Writes
    ) {
        throw "BS-2 target call $($index + 1) changed."
    }
}

# Slot $53 is deliberately occupied by the event seed, so the authentic
# emitter obtains the next dynamic slot, $54.
Assert-Write $writes 1 "04" "4FC2" "1054" "C2" `
    "Spawned target dispatch type"
Assert-Write $writes 1 "04" "4FC8" "11D4" "F8" `
    "Spawned target horizontal position"
Assert-Write $writes 1 "04" "4FD4" "1354" "30" `
    "Spawned target vertical position"
Assert-Write $writes 1 "04" "4FE0" "1954" "1D" `
    "Spawned target animation"
Assert-Write $writes 1 "04" "4FE6" "1754" "11" `
    "Spawned target collision shape"
Assert-Write $writes 1 "04" "4FF1" "1854" "10" `
    "Spawned target collision selector"
Assert-Write $writes 1 "04" "4FF7" "1D54" "81" `
    "Spawned target completion selector"
Assert-Write $writes 1 "04" "4FFC" "10D4" "84" `
    "Spawned target type"
Assert-Write $writes 1 "04" "5001" "1DD4" "10" `
    "Spawned target entrance delay"
Assert-Write $writes 1 "04" "5006" "1C54" "01" `
    "Spawned target hit count"
Assert-Write $writes 1 "04" "500B" "1F54" "51" `
    "Spawned target dispatch low byte"
Assert-Write $writes 1 "04" "5010" "1FD4" "50" `
    "Spawned target dispatch high byte"
Assert-Write $writes 1 "04" "501A" "0D26" "01" `
    "Spawned target lane-count increment"
Assert-Write $writes 2 "04" "4F02" "1A34" "30" `
    "Emitter timer reload"
Assert-Write $writes 2 "04" "4F08" "1B34" "04" `
    "Emitter batch decrement"

# The seeded slot $53 enters the real contact state. It clears bit 7, awards
# 50 units, couples response selector $35, balances the lane counter, and
# reaches the generic actor clear.
Assert-Write $writes 3 "04" "5093" "1853" "10" `
    "Target collision clear"
Assert-Write $writes 3 "04" "509B" "002F" "32" `
    "Target score input"
Assert-Write $writes 3 "00" "F56B" "0DB7" "32" `
    "Target bonus low byte"
Assert-Write $writes 3 "00" "F573" "0DB8" "00" `
    "Target bonus middle byte"
Assert-Write $writes 3 "00" "F57A" "0DB9" "00" `
    "Target bonus high byte"
Assert-Write $writes 3 "04" "50A4" "0D6E" "35" `
    "Target response selector"
Assert-Write $writes 3 "04" "50B4" "0D26" "00" `
    "Target lane-count decrement"
Assert-Write $writes 3 "01" "C581" "1053" "00" `
    "Target actor release"

$objectiveWrites = @(
    $states | Where-Object { $_.logical_address -eq "2D2E" }
)
if (
    $objectiveWrites.Count -ne 2 -or
    $objectiveWrites[0].frame -ne "1" -or
    $objectiveWrites[0].value -ne "00" -or
    $objectiveWrites[1].frame -ne "1342" -or
    $objectiveWrites[1].instruction_physical_bank -ne "04" -or
    $objectiveWrites[1].instruction_logical_pc -ne "4E91" -or
    $objectiveWrites[1].value -ne "1C"
) {
    throw "BS-2 objective counter timeline changed."
}

Write-Output "BS-2 emitter and target semantics verified."
Write-Output (
    "Controller: slot 2C initializes objective counter 2D2E to 28."
)
Write-Output (
    "Emitter: slot 34 reloads to 48 ticks, decrements its five-target " +
    "batch, and allocates target slot 54."
)
Write-Output (
    "Target contact: slot 53 awards 50 stored units, selects response 35, " +
    "balances lane counter 2D26, and releases."
)
