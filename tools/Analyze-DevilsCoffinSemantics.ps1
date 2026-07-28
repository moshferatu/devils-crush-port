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

# The manual's Devil's Coffin opens the return gate back to the Shooter.
# Fixed slot $0F is the only record at the matching right-middle-table
# position and its completion clears the shared $53.3 session/return state.
Assert-RomBytes $rom 0x004617 @(
    0x00, 0x0F, 0xE1, 0x63, 0xDD, 0x00,
    0x01, 0x01, 0x11, 0x03, 0x83, 0x00
) "Devil's Coffin layout record"

# Initialization clears the collision marker, resets the visual index, and
# installs 63F1. While session bit $53.3 is set, the active type is refreshed
# to $83. A collision starts the first three-step appearance pass.
Assert-RomBytes $rom 0x0063E1 @(
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x09, 0x40, 0x9D, 0x00, 0x38,
    0x9E, 0x80, 0x39, 0x20, 0x18, 0xC0,
    0xA5, 0x53, 0x29, 0x08, 0xF0, 0x05,
    0xA9, 0x83, 0x9D, 0x00, 0x30,
    0xBD, 0x00, 0x38, 0x30, 0x01, 0x60,
    0xA9, 0x01, 0x9D, 0x00, 0x3A, 0x20, 0x18, 0xC0
) "Devil's Coffin initialization and collision gate"

# 640A advances appearance 1 -> 2 -> 3 on four-tick intervals, clears the
# collision marker, and hands off through 6432 to the reverse pass.
Assert-RomBytes $rom 0x00640A @(
    0xDE, 0x00, 0x3A, 0xD0, 0x17,
    0xA9, 0x04, 0x9D, 0x00, 0x3A,
    0xFE, 0x80, 0x39,
    0xBD, 0x00, 0x38, 0x09, 0x40, 0x9D, 0x00, 0x38,
    0xBD, 0x80, 0x39, 0xC9, 0x03, 0xB0, 0x01, 0x60,
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x9D, 0x00, 0x38,
    0x20, 0x18, 0xC0
) "Devil's Coffin opening appearance pass"
Assert-RomBytes $rom 0x006432 @(
    0xBD, 0x00, 0x38, 0x10, 0x06,
    0x29, 0x7F, 0x9D, 0x00, 0x38, 0x60,
    0xA9, 0x01, 0x9D, 0x00, 0x3A, 0x20, 0x18, 0xC0
) "Devil's Coffin reverse-pass handoff"

# The reverse pass advances 2 -> 1 -> 0, clears session bit $53.3, changes
# the actor type to $82, and returns to the initializer.
Assert-RomBytes $rom 0x006445 @(
    0xDE, 0x00, 0x3A, 0xD0, 0x12,
    0xA9, 0x04, 0x9D, 0x00, 0x3A,
    0xBD, 0x00, 0x38, 0x09, 0x40, 0x9D, 0x00, 0x38,
    0xDE, 0x80, 0x39, 0xF0, 0x01, 0x60,
    0xA5, 0x53, 0x29, 0xF7, 0x85, 0x53,
    0xA9, 0x82, 0x9D, 0x00, 0x30, 0x4C, 0xE1, 0x63
) "Devil's Coffin completion and return-gate state"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 684 -or
    $writes.Count -ne 355 -or
    $states.Count -ne 372
) {
    throw (
        "Expected 684 Devil's-Coffin calls, 355 scoped writes, and " +
        "372 selected state writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("63E1", "63F1", "640A", "6432", "6445") -or
        $call.input_x -ne "0F" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Devil's-Coffin call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Devil's Coffin changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

$initializers = @($calls | Where-Object { $_.logical_pc -eq "63E1" })
if (
    $initializers.Count -ne 3 -or
    ($initializers.frame -join ",") -ne "871,1263,1316" -or
    ($initializers.body_master_cycles | Select-Object -Unique) -ne "249" -or
    ($initializers.write_count | Select-Object -Unique) -ne "9"
) {
    throw "Devil's-Coffin initialization schedule changed."
}

$steady = @($calls | Where-Object { $_.logical_pc -eq "63F1" })
$steadyNoWrite = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 51 -and
            [int] $_.write_count -eq 0
        }
)
$steadyTypeWrite = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 66 -and
            [int] $_.write_count -eq 1
        }
)
$steadyInterrupts = @(
    $steady |
        Where-Object { $_.return_instruction_pc -eq "6401" -and
            [int] $_.write_count -gt 1 }
)
$collisions = @(
    $steady |
        Where-Object { $_.return_instruction_pc -eq "C5E9" }
)
if (
    $steady.Count -ne 643 -or
    [int] $steady[0].frame -ne 1227 -or
    [int] $steady[-1].frame -ne 3600 -or
    $steadyNoWrite.Count -ne 611 -or
    $steadyTypeWrite.Count -ne 17 -or
    $steadyInterrupts.Count -ne 13 -or
    ($collisions.frame -join ",") -ne "1244,1297" -or
    ($collisions.body_master_cycles -join ",") -ne "285,270" -or
    ($collisions.write_count -join ",") -ne "9,8"
) {
    throw "Devil's-Coffin steady or collision schedule changed."
}

$opening = @($calls | Where-Object { $_.logical_pc -eq "640A" })
$handoff = @($calls | Where-Object { $_.logical_pc -eq "6432" })
$closing = @($calls | Where-Object { $_.logical_pc -eq "6445" })
if (
    $opening.Count -ne 18 -or
    ($opening | Where-Object {
        [int64] $_.body_master_cycles -eq 33 -and
        [int] $_.write_count -eq 1
    }).Count -ne 12 -or
    ($opening | Where-Object {
        [int64] $_.body_master_cycles -eq 132 -and
        [int] $_.write_count -eq 4
    }).Count -ne 4 -or
    ($opening | Where-Object {
        [int64] $_.body_master_cycles -eq 366 -and
        [int] $_.write_count -eq 12
    }).Count -ne 2 -or
    $handoff.Count -ne 2 -or
    ($handoff.frame -join ",") -ne "1254,1307" -or
    ($handoff.body_master_cycles | Select-Object -Unique) -ne "240" -or
    ($handoff.write_count | Select-Object -Unique) -ne "8" -or
    $closing.Count -ne 18 -or
    ($closing | Where-Object {
        [int64] $_.body_master_cycles -eq 33 -and
        [int] $_.write_count -eq 1
    }).Count -ne 12 -or
    ($closing | Where-Object {
        [int64] $_.body_master_cycles -eq 111 -and
        [int] $_.write_count -eq 4
    }).Count -ne 4 -or
    ($closing | Where-Object {
        [int64] $_.body_master_cycles -eq 429 -and
        [int] $_.write_count -eq 15
    }).Count -ne 2
) {
    throw "Devil's-Coffin two-pass animation schedule changed."
}

$typeRefresh = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "63F9"
        }
)
$completionType = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "6465"
        }
)
$sessionClears = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "6461"
        }
)
if (
    $typeRefresh.Count -ne 18 -or
    [int] $typeRefresh[0].frame -ne 1227 -or
    [int] $typeRefresh[-1].frame -ne 1244 -or
    ($typeRefresh.value | Select-Object -Unique) -ne "83" -or
    ($completionType.frame -join ",") -ne "1263,1316" -or
    ($completionType.value -join ",") -ne "82,82" -or
    ($sessionClears.frame -join ",") -ne "1263,1316" -or
    ($sessionClears.value -join ",") -ne "00,00"
) {
    throw "Devil's-Coffin type or session-state timeline changed."
}

$openingAppearance = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "6414"
        }
)
$closingAppearance = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "6457"
        }
)
if (
    ($openingAppearance.frame -join ",") -ne
        "1245,1249,1253,1298,1302,1306" -or
    ($openingAppearance.value -join ",") -ne "01,02,03,01,02,03" -or
    ($closingAppearance.frame -join ",") -ne
        "1255,1259,1263,1308,1312,1316" -or
    ($closingAppearance.value -join ",") -ne "02,01,00,02,01,00"
) {
    throw "Devil's-Coffin appearance cycle changed."
}

$collisionFlags = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "CBDF" -and
            $_.logical_address -eq "380F"
        }
)
if (
    ($collisionFlags.frame -join ",") -ne "1244,1297" -or
    ($collisionFlags.value -join ",") -ne "80,80"
) {
    throw "Devil's-Coffin collision inputs changed."
}

$pointerLows = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            $_.logical_address -eq "3F0F"
        }
)
$pointerHighs = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6" -and
            $_.logical_address -eq "3F8F"
        }
)
if (
    ($pointerLows.frame -join ",") -ne
        "871,1244,1253,1254,1263,1297,1306,1307,1316" -or
    ($pointerLows.value -join ",") -ne
        "F1,0A,32,45,F1,0A,32,45,F1" -or
    ($pointerHighs.frame -join ",") -ne
        "871,1244,1253,1254,1263,1297,1306,1307,1316" -or
    ($pointerHighs.value -join ",") -ne
        "63,64,64,64,63,64,64,64,63"
) {
    throw "Devil's-Coffin dispatch timeline changed."
}

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
            $_.instruction_logical_pc -eq "ECFD" -and
            $_.work_ram_offset -eq "002A"
        }
)
if ($scoreWrites.Count -ne 0 -or $audioWrites.Count -ne 0) {
    throw "Devil's Coffin unexpectedly owned score or audio writes."
}

Write-Output "Devil's Coffin semantics verified."
Write-Output (
    "Fixed return-gate target: slot 0F at (221,257), initializer 03:63E1."
)
Write-Output (
    "Routed contacts: frames 1244 and 1297; each runs appearance " +
    "0-1-2-3-2-1-0 and returns to 03:63F1."
)
Write-Output (
    "Completion clears session bit `$53.3 and changes type 83 to 82; " +
    "the family owns no score or audio command."
)
Write-Output (
    "Normalized samples: 03:63F1/640A/6432/6445 all verified."
)
