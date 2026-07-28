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
    [string] $ActorWritePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-RomBytes {
    param(
        [byte[]] $Rom,
        [int] $Offset,
        [byte[]] $Expected,
        [string] $Label
    )

    if ($Offset -lt 0 -or $Offset + $Expected.Count -gt $Rom.Count) {
        throw "$Label is outside the ROM."
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $Expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}: expected {1:X2}, " +
                "found {2:X2}."
            ) -f (
                $Offset + $index
            ), $Expected[$index], $Rom[$Offset + $index]
        }
    }
}

function Import-TraceCsv {
    param(
        [string] $Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Trace does not exist: $Path"
    }
    return @(
        Get-Content -LiteralPath $Path |
            Where-Object { -not $_.StartsWith("#") } |
            ConvertFrom-Csv
    )
}

function Assert-Histogram {
    param(
        [object[]] $Rows,
        [string] $Property,
        [hashtable] $Expected,
        [string] $Label
    )

    $actual = @{}
    foreach ($group in @($Rows | Group-Object -Property $Property)) {
        $actual[$group.Name] = $group.Count
    }
    if ($actual.Count -ne $Expected.Count) {
        throw "$Label changed population."
    }
    foreach ($key in $Expected.Keys) {
        if (
            -not $actual.ContainsKey($key) -or
            $actual[$key] -ne $Expected[$key]
        ) {
            throw (
                "$Label changed ${key}: expected $($Expected[$key]), " +
                "found $($actual[$key])."
            )
        }
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The two category-2 fixed records are a mirrored pair at the lower-table
# bumper positions. Their distinct initializers set side index 0/1 before
# converging on the same 03:78C7 steady update.
Assert-RomBytes $rom 0x004653 @(
    0x02, 0x14, 0x9A, 0x78, 0xBE, 0x00,
    0x5C, 0x02, 0x0E, 0x03, 0x82, 0x00,
    0x02, 0x15, 0xA4, 0x78, 0x4A, 0x00,
    0x5C, 0x02, 0x0F, 0x03, 0x82, 0x01
) "Bumper layout records"
Assert-RomBytes $rom 0x00789A @(
    0x9E, 0x00, 0x3E, 0xA9, 0x86, 0x9D, 0x00, 0x3D, 0x80, 0x0A,
    0xA9, 0x01, 0x9D, 0x00, 0x3E, 0xA9, 0x87, 0x9D, 0x00, 0x3D,
    0xA9, 0x50, 0x9D, 0x00, 0x38,
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x9D, 0x00, 0x38,
    0x9E, 0x80, 0x3E, 0x9E, 0x00, 0x3B, 0x9E, 0x80, 0x3B,
    0x20, 0x18, 0xC0
) "Bumper initializers"

# Each hit clears the collision result, advances the phase at $3E80,X, and
# dispatches through a nine-entry table: six primary appearances, two final
# damage appearances, then destruction.
Assert-RomBytes $rom 0x0078C7 @(
    0xBD, 0x00, 0x38, 0x10, 0x1A,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xBD, 0x80, 0x3E, 0xFE, 0x80, 0x3E,
    0x0A, 0xA8,
    0xB9, 0x86, 0x79, 0x85, 0x2F,
    0xB9, 0x87, 0x79, 0x85, 0x30,
    0x6C, 0x2F, 0x20, 0x60
) "Bumper steady dispatcher"
Assert-RomBytes $rom 0x0078E7 @(
    0xA9, 0x0A, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x20, 0x20, 0x87, 0xC0,
    0xFE, 0x00, 0x3B,
    0xBD, 0x00, 0x3B, 0x20, 0x39, 0x7A, 0x4C, 0xF6, 0x7B,
    0xA9, 0x0A, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0xF6, 0x7B,
    0xA9, 0x20, 0x20, 0x87, 0xC0,
    0xBD, 0x80, 0x3B, 0xC9, 0x02, 0xF0, 0x11,
    0xFE, 0x80, 0x3B,
    0xBD, 0x80, 0x3B, 0x9D, 0x80, 0x39,
    0xBD, 0x00, 0x38, 0x09, 0x40, 0x9D, 0x00, 0x38, 0x60,
    0xA9, 0xC8, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x66, 0xC0, 0x20, 0x44, 0x41,
    0xA9, 0x03, 0x9D, 0x80, 0x39,
    0xBD, 0x00, 0x38, 0x09, 0x40, 0x9D, 0x00, 0x38,
    0xA9, 0x04, 0x20, 0x21, 0xC0, 0x20, 0x18, 0xC0,
    0x20, 0x98, 0x79,
    0xA9, 0xC0, 0x20, 0x21, 0xC0, 0x20, 0x18, 0xC0,
    0xA9, 0x01, 0x9D, 0x00, 0x3A, 0x20, 0x18, 0xC0,
    0xDE, 0x00, 0x3A, 0xD0, 0x10,
    0xA9, 0x04, 0x9D, 0x00, 0x3A,
    0xBD, 0x00, 0x3B, 0xF0, 0x07,
    0xDE, 0x00, 0x3B, 0x20, 0x39, 0x7A, 0x60,
    0x9E, 0x80, 0x39,
    0xBD, 0x00, 0x38, 0x09, 0x40, 0x9D, 0x00, 0x38,
    0x4C, 0xB3, 0x78
) "Bumper hit, destruction, and reset states"
Assert-RomBytes $rom 0x007986 @(
    0xE7, 0x78, 0xE7, 0x78, 0xE7, 0x78,
    0xE7, 0x78, 0xE7, 0x78, 0xE7, 0x78,
    0x01, 0x79, 0x01, 0x79, 0x2B, 0x79
) "Bumper nine-hit dispatch table"

# Terminal destruction calls a six-iteration allocator. Each child is type
# $83, begins at 03:7A71, copies the bumper position, and increments the
# side-selected live count at $2D11,Y.
Assert-RomBytes $rom 0x007998 @(
    0xBC, 0x00, 0x3E, 0x62, 0x99, 0x11, 0x2D,
    0xBD, 0x00, 0x3E, 0x0A, 0x0A, 0x0A, 0x85, 0x27,
    0xA9, 0x06, 0x85, 0x26,
    0x20, 0x24, 0xC0, 0xB0, 0x1D,
    0x20, 0xDE, 0x79,
    0xDA, 0xA6, 0x27, 0xBD, 0xCE, 0x79, 0x99, 0x00, 0x37, 0xFA,
    0xBC, 0x00, 0x3E, 0xB9, 0x11, 0x2D, 0x1A, 0x99, 0x11, 0x2D,
    0xE6, 0x27, 0xC6, 0x26, 0xD0, 0xDE, 0x60,
    0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22,
    0x05, 0x04, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x04, 0x04
) "Bumper six-child spawner"
Assert-RomBytes $rom 0x0079DE @(
    0xA9, 0x71, 0x99, 0x00, 0x3F,
    0xA9, 0x7A, 0x99, 0x80, 0x3F,
    0xA9, 0x83, 0x99, 0x00, 0x30,
    0xBD, 0x80, 0x31, 0x99, 0x80, 0x31,
    0xBD, 0x00, 0x32, 0x99, 0x00, 0x32,
    0xBD, 0x00, 0x33, 0x99, 0x00, 0x33,
    0xBD, 0x80, 0x33, 0x99, 0x80, 0x33,
    0xBD, 0x00, 0x3E, 0x99, 0x80, 0x3C,
    0xA9, 0x1B, 0x99, 0x00, 0x39,
    0xA9, 0x10, 0x99, 0x00, 0x38,
    0xA9, 0x8C, 0x99, 0x00, 0x3D,
    0xA9, 0x00, 0x99, 0x80, 0x39,
    0xA9, 0x84, 0x99, 0x80, 0x30,
    0xA9, 0x80, 0x99, 0x80, 0x38,
    0xA9, 0x04, 0x99, 0x00, 0x3A,
    0xA9, 0x50, 0x99, 0x00, 0x3B,
    0xA9, 0x01, 0x99, 0x00, 0x3C, 0x60
) "Bumper-child initializer"
Assert-RomBytes $rom 0x00202A @(0x4C, 0x4F, 0xF5) "Score entry"
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 3468 -or
    $writes.Count -ne 3127 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 3,468 bumper calls, 3,127 scoped writes, and 3,872 " +
        "actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne "78C7" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Bumper bounded-call identity changed at sequence $($index + 1)."
    }
    # Animation helpers intentionally remap MPR4/MPR5 on hit paths.
    foreach ($mpr in @(0, 1, 2, 3, 6, 7)) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Bumper changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "input_x" @{
    "14" = 1734
    "15" = 1734
} "Bumper slot"
Assert-Histogram $calls "return_instruction_pc" @{
    "78E6" = 3457
    "7C33" = 9
    "792A" = 2
} "Bumper return"

$bodyCycles = @($calls | ForEach-Object { [int64] $_.body_master_cycles })
if (
    [int] $calls[0].frame -ne 872 -or
    [int] $calls[-1].frame -ne 3562 -or
    ($bodyCycles | Measure-Object -Minimum).Minimum -ne 27 -or
    ($bodyCycles | Measure-Object -Maximum).Maximum -ne 17781 -or
    [int64] $calls[0].body_master_cycles -ne 27 -or
    [int] $calls[0].write_count -ne 0
) {
    throw "Bumper frame, timing, or replay sample changed."
}

$hitMarkers = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.work_ram_offset -eq "002F" -and
            $_.value -eq "0A" -and
            $_.instruction_logical_pc -in @("78E9", "7903")
        }
)
$expectedHitSequences = @(602, 1133, 1374, 1552, 1571, 1596, 1970, 2276,
    2602, 2647, 3036)
$expectedHitFrames = @(2125, 2391, 2511, 2600, 2610, 2622, 2809, 2962,
    3125, 3148, 3342)
$expectedHitSlots = @("15", "14", "15", "15", "14", "15", "15", "15",
    "15", "14", "15")
$expectedPhases = @("01", "01", "02", "03", "02", "04", "05", "06",
    "07", "03", "08")
if ($hitMarkers.Count -ne 11) {
    throw "Expected 11 routed bumper hits."
}

for ($index = 0; $index -lt $hitMarkers.Count; ++$index) {
    $sequence = [int] $hitMarkers[$index].call_sequence
    $call = $calls[$sequence - 1]
    $slot = [Convert]::ToInt32($expectedHitSlots[$index], 16)
    $callWrites = @(
        $writes |
            Where-Object { [int] $_.call_sequence -eq $sequence }
    )
    $phaseWrites = @(
        $callWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq "78D4" -and
                $_.work_ram_offset -eq
                    ("{0:X4}" -f (0x1E80 + $slot))
            }
    )
    $audioWrites = @(
        $callWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq "ECFD" -and
                $_.work_ram_offset -eq "002A" -and
                $_.value -eq "20"
            }
    )
    $collisionClears = @(
        $callWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq "78CE" -and
                $_.work_ram_offset -eq
                    ("{0:X4}" -f (0x1800 + $slot))
            }
    )
    if (
        $sequence -ne $expectedHitSequences[$index] -or
        [int] $call.frame -ne $expectedHitFrames[$index] -or
        $call.input_x -ne $expectedHitSlots[$index] -or
        $phaseWrites.Count -ne 1 -or
        $phaseWrites[0].value -ne $expectedPhases[$index] -or
        $audioWrites.Count -ne 1 -or
        $collisionClears.Count -ne 1
    ) {
        throw "Bumper hit $($index + 1) changed."
    }
}

$primaryCounterWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "78F5"
        }
)
$secondaryCounterWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7919"
        }
)
$terminalAwardWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "792D"
        }
)
if (
    $primaryCounterWrites.Count -ne 9 -or
    ($primaryCounterWrites.value -join ",") -ne
        "01,01,02,03,02,04,05,06,03" -or
    $secondaryCounterWrites.Count -ne 2 -or
    ($secondaryCounterWrites.value -join ",") -ne "01,02" -or
    $terminalAwardWrites.Count -ne 0
) {
    throw "Bumper primary, secondary, or terminal phase population changed."
}

$zeroWriteCalls = @($calls | Where-Object { [int] $_.write_count -eq 0 })
$interruptOnlyCalls = @(
    $calls |
        Where-Object {
            [int] $_.write_count -gt 0 -and
            [int] $_.sequence -notin $expectedHitSequences
        }
)
if ($zeroWriteCalls.Count -ne 3419 -or $interruptOnlyCalls.Count -ne 38) {
    throw "Bumper no-hit/interrupt population changed."
}

# Guard both fixed layout identities. No 79DE child initialization is
# expected because neither bumper reaches the ninth hit on this route.
$bumperTypes = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4131" -and
            $_.logical_address -in @("3014", "3015") -and
            $_.value -eq "82"
        }
)
$bumperLow = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            $_.logical_address -in @("3F14", "3F15") -and
            $_.value -eq "C7"
        }
)
$bumperHigh = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6" -and
            $_.logical_address -in @("3F94", "3F95") -and
            $_.value -eq "78"
        }
)
$childIdentityWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -in @("79E2", "79E7", "79EC")
        }
)
if (
    ($bumperTypes.logical_address -join ",") -ne "3014,3015" -or
    ($bumperLow.logical_address -join ",") -ne "3F14,3F15" -or
    ($bumperHigh.logical_address -join ",") -ne "3F94,3F95" -or
    $childIdentityWrites.Count -ne 0
) {
    throw "Bumper actor identity or inactive child path changed."
}

Write-Output "Bumper family semantics verified."
Write-Output (
    "Fixed bumpers: slots 14/15 at (190/74,604), dispatch 03:78C7, " +
    "1734 calls each."
)
Write-Output (
    "Hit rule: 11 routed hits award 10 and emit command 20; " +
    "slot 15 reaches phase 8 and slot 14 reaches phase 3."
)
Write-Output (
    "Terminal rule: phase 9 adds 200 and spawns six type-83 children " +
    "at 03:7A71; the route does not reach destruction."
)
Write-Output (
    "Observed score contribution: 110 stored units; first normalized " +
    "no-hit sample 03:78C7@1 is frame 872, 27 cycles, 0 writes."
)
