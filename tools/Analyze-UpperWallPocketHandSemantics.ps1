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

# The guide says the two upper 4 Stars sit behind hands that open and close.
# Slots $40/$41 are the only mirrored records directly above upper-pocket
# slots $42/$43; their initial appearance values $14/$15 preserve handedness.
Assert-RomHex $rom 0x004863 @"
00 40 72 59 48 00 37 00 14 C2 82 00
00 41 72 59 E8 00 37 00 15 C2 82 00
00 42 20 4A 56 00 4C 00 16 82 84 00
00 43 32 4A F6 00 4C 00 16 82 84 00
"@ "Upper Wall-Pocket hands and pockets"

# Initialization installs 02:5975. The live scheduler hides the collision
# surface, derives a long delay from RNG plus shared byte $0080, and
# waits on a 16-bit timer before consulting the pocket-region selector.
Assert-RomHex $rom 0x005972 @"
20 18 C0 BD 80 30 09 40 9D 80 30 20 81 C0 29 3F
18 6D 80 00 9D 00 3B A9 01 9D 80 3B 20 18 C0 BD
00 3B D0 03 DE 80 3B 3A 9D 00 3B 1D 80 3B D0 52
20 18 C0
"@ "Upper-hand initialization and delayed scheduler"

# A nonzero $2D33 keeps the hands dormant. Upper-pocket setup clears that
# selector; the hand then exposes a temporary type-$83 collision surface,
# passes response selector 4 to the common ball helper, and waits until the
# collision marker clears before beginning the close/open animation.
Assert-RomHex $rom 0x0059A5 @"
AD 33 2D D0 4A BD 80 30 29 BF 9D 80 30 BD 00 38
29 7F 9D 00 38 A9 83 9D 00 30 A9 04 20 21 C0 20
18 C0 BD 00 38 10 07 29 7F 9D 00 38 80 EC A9 82
9D 00 30 A9 01 9D 00 3A 20 18 C0 DE 00 3A D0 0F
A9 04 9D 00 3A FE 80 39 BD 80 39 C9 02 F0 01 60
"@ "Upper-hand collision wait and closing pass"

# At the end of the two-step pass, a bounded random value $B0-$EF is passed
# to the common ball-response helper. The region selector determines whether
# one or two dispatch transitions are applied before the reverse pass, which
# restores appearance to zero and returns to 02:5975.
Assert-RomHex $rom 0x0059F5 @"
20 81 C0 29 3F 18 69 B0 20 21 C0 AD 33 2D D0 03
20 18 C0 20 18 C0 A9 01 9D 00 3A BD 80 30 09 40
9D 80 30 20 18 C0 DE 00 3A D0 10 A9 04 9D 00 3A
BD 80 39 D0 03 4C 75 59 DE 80 39 60
"@ "Upper-hand response and reverse pass"

# Upper Wall-Pocket initializers carry selectors VII/II in $3780,X. Common
# setup clears $2D33 for these values, while the lower selectors IV/VI retain
# the alternate region state.
Assert-RomHex $rom 0x004A20 @"
A9 03 9D 80 3C A9 03 9D 00 37 A9 07 9D 80 37 4C
6C 4A
"@ "Upper-left Wall-Pocket initialization"
Assert-RomHex $rom 0x004A32 @"
A9 0A 9D 80 3C A9 06 9D 00 37 A9 01 9D 80 37 4C
6C 4A
"@ "Upper-right Wall-Pocket initialization"
Assert-RomHex $rom 0x004A6C @"
BD 80 37 C9 03 F0 07 C9 05 F0 03 9C 33 2D
"@ "Upper-pocket region-selector clear"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 2 -or
    $writes.Count -ne 14 -or
    $states.Count -ne 1129
) {
    throw (
        "Expected two upper-hand initializers, 14 scoped writes, and " +
        "1,129 selected state writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $expectedSlot = "{0:X2}" -f (0x40 + $index)
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne 872 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -ne "5972" -or
        $call.caller_pc -ne "4096" -or
        $call.return_pc -ne "4099" -or
        $call.return_instruction_pc -ne "C5E9" -or
        [int64] $call.body_master_cycles -ne 192 -or
        $call.input_a -ne "00" -or
        $call.input_x -ne $expectedSlot -or
        $call.output_a -ne "59" -or
        $call.input_x -ne $call.output_x -or
        $call.input_y -ne $call.output_y -or
        $call.input_sp -ne $call.output_sp -or
        [int] $call.write_count -ne 7
    ) {
        throw "Upper-hand initializer changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Upper hand changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

$pointerLows = @(
    $states |
        Where-Object {
            $_.frame -eq "872" -and
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            $_.logical_address -in @("3F40", "3F41")
        }
)
$pointerHighs = @(
    $states |
        Where-Object {
            $_.frame -eq "872" -and
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6" -and
            $_.logical_address -in @("3FC0", "3FC1")
        }
)
if (
    $pointerLows.Count -ne 2 -or
    ($pointerLows.value | Select-Object -Unique) -ne "75" -or
    $pointerHighs.Count -ne 2 -or
    ($pointerHighs.value | Select-Object -Unique) -ne "59"
) {
    throw "Upper-hand live dispatch installation changed."
}

$regionWrites = @(
    $states | Where-Object { $_.logical_address -eq "2D33" }
)
if (
    $regionWrites.Count -ne 3 -or
    ($regionWrites.value | Select-Object -Unique) -ne "00" -or
    @($regionWrites | Where-Object {
        $_.frame -eq "872" -and
        $_.instruction_physical_bank -eq "02" -and
        $_.instruction_logical_pc -eq "4A77"
    }).Count -ne 2
) {
    throw "Upper-hand Wall-Pocket region-selector timeline changed."
}

# The route initializes both hands but never camera-dispatches 02:5975.
# Consequently none of the hand's inactive response states submit score or
# audio commands.
$scoreWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -in @("F587", "F58F", "F597", "F5A5")
        }
)
$audioWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "ECFD"
        }
)
if ($scoreWrites.Count -ne 0 -or $audioWrites.Count -ne 0) {
    throw "Upper Wall-Pocket hands unexpectedly exercised score or audio."
}

Write-Output "Upper Wall-Pocket hand semantics verified."
Write-Output (
    "Fixed mirrored hands: slots 40/41 at (72,55)/(232,55), " +
    "immediately above pockets 42/43."
)
Write-Output (
    "ROM-bounded gate: 2D33 selects the upper pair, type 83 exposes " +
    "collision, and a two-step pass returns to 02:5975."
)
Write-Output (
    "Routed coverage: two frame-872 initializers; live 02:5975 remains inactive."
)
Write-Output "The hand family owns no score or audio command."
Write-Output "First normalized sample: 02:5972@1, frame 872, 192 cycles."
