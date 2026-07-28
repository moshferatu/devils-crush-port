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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The active-table ball handler integrates slot $00, tests its signed
# 16-bit Y coordinate against $02D0 in normal-table mode, and enters the
# drain/turn transition when the comparison sets carry.
Assert-RomBytes $rom 0x002765 @(
    0x20, 0xF3, 0xC8,
    0x20, 0x09, 0xDD,
    0x20, 0x6E, 0xDD,
    0xB0, 0x03,
    0x4C, 0x95, 0xDC
) "C765 active-ball drain gate"
Assert-RomBytes $rom 0x003D6E @(
    0xA5, 0x55, 0xD0, 0x11,
    0x38, 0xBD, 0x00, 0x33, 0xE9, 0xD0,
    0xBD, 0x80, 0x33, 0xE9, 0x02,
    0x90, 0x02, 0x38, 0x60, 0x18, 0x60
) "DD6E normal-table Y threshold"

# A remaining ball clears session transition bit 1, invokes export 3 in
# bank 2, clears/reinitializes the ball, and marks the table dirty.
Assert-RomBytes $rom 0x002812 @(
    0xAD, 0x52, 0x2D, 0xF0, 0x21,
    0xA5, 0x53, 0x29, 0xFD, 0x85, 0x53, 0xDA,
    0xA9, 0x03, 0x85, 0x2F,
    0xA9, 0x02, 0x85, 0x30,
    0x20, 0x41, 0xE2, 0xFA,
    0x20, 0x74, 0xC5,
    0xAD, 0xC0, 0x2C, 0x09, 0x01, 0x8D, 0xC0, 0x2C,
    0x4C, 0x55, 0xC6
) "C812 remaining-ball selective reload"
Assert-RomBytes $rom 0x0040A8 @(
    0xA9, 0xDB, 0x85, 0x2F, 0xA9, 0x45, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0x15,
    0xC9, 0x01, 0xD0, 0x03, 0x20, 0xF6, 0x40
) "40A8 category-1 selector"

$records = [System.Collections.Generic.List[byte[]]]::new()
$tableOffset = 0x0045DB
while ($rom[$tableOffset] -ne 0xFF) {
    $record = [byte[]] $rom[$tableOffset..($tableOffset + 11)]
    if ($record[0] -eq 1) {
        $records.Add($record)
    }
    $tableOffset += 12
}
if ($records.Count -ne 7) {
    throw "Expected seven category-1 scene records."
}

$expectedSlots = @(0x2F, 0x36, 0x3A, 0x3B, 0x3C, 0x3D, 0x4C)
for ($index = 0; $index -lt $expectedSlots.Count; ++$index) {
    if ($records[$index][1] -ne $expectedSlots[$index]) {
        throw "Category-1 scene slot order changed."
    }
}

$calls = @(Import-TraceCsv $CallPath)
if ($calls.Count -ne 1) {
    throw "Expected one category-1 scene-layout call, found $($calls.Count)."
}
$call = $calls[0]
if (
    $call.physical_bank -ne "02" -or
    $call.logical_pc -ne "4003" -or
    $call.caller_pc -ne "E252" -or
    $call.return_pc -ne "E255" -or
    $call.return_physical_bank -ne "02" -or
    $call.return_instruction_pc -ne "40CE" -or
    [int] $call.frame -ne 3287 -or
    [int64] $call.body_master_cycles -ne 98931 -or
    [int] $call.write_count -ne 2095
) {
    throw "Category-1 bounded call changed."
}
if (
    $call.input_mpr2 -ne "02" -or
    $call.output_mpr2 -ne "02" -or
    $call.input_mpr3 -ne "03" -or
    $call.output_mpr3 -ne "03"
) {
    throw "Category-1 mapped ROM banks changed."
}

$writes = @(Import-TraceCsv $WritePath)
if ($writes.Count -ne 2095) {
    throw "Expected 2,095 scoped category-1 writes."
}

$directFields = @(
    [pscustomobject] @{ Pc = "410D"; Base = 0x1180; Byte = 4 },
    [pscustomobject] @{ Pc = "4113"; Base = 0x1200; Byte = 5 },
    [pscustomobject] @{ Pc = "4119"; Base = 0x1300; Byte = 6 },
    [pscustomobject] @{ Pc = "411F"; Base = 0x1380; Byte = 7 },
    [pscustomobject] @{ Pc = "4125"; Base = 0x1900; Byte = 8 },
    [pscustomobject] @{ Pc = "412B"; Base = 0x1080; Byte = 9 },
    [pscustomobject] @{ Pc = "4131"; Base = 0x1000; Byte = 10 }
)
foreach ($field in $directFields) {
    $fieldWrites = @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -eq $field.Pc
            }
    )
    if ($fieldWrites.Count -ne 7) {
        throw "Category-1 field $($field.Pc) changed write count."
    }
    for ($index = 0; $index -lt 7; ++$index) {
        $record = $records[$index]
        if (
            [Convert]::ToInt32(
                $fieldWrites[$index].work_ram_offset,
                16
            ) -ne $field.Base + $record[1] -or
            [Convert]::ToInt32(
                $fieldWrites[$index].value,
                16
            ) -ne $record[$field.Byte]
        ) {
            throw (
                "Category-1 field $($field.Pc) changed record " +
                "$($index + 1)."
            )
        }
    }
}

$initializerLowWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4101"
        }
)
$initializerHighWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4107"
        }
)
if (
    $initializerLowWrites.Count -ne 7 -or
    $initializerHighWrites.Count -ne 7
) {
    throw "Category-1 initializer scratch writes changed count."
}
for ($index = 0; $index -lt 7; ++$index) {
    if (
        [Convert]::ToInt32($initializerLowWrites[$index].value, 16) -ne
            $records[$index][2] -or
        [Convert]::ToInt32($initializerHighWrites[$index].value, 16) -ne
            $records[$index][3]
    ) {
        throw "Category-1 initializer changed at record $($index + 1)."
    }
}

# Interrupts inside this cross-frame call may clear unrelated dynamic slots.
# Require the category loader's complete 32-array clear for each selected
# fixed slot without assigning those interrupt writes to the loader.
$clearWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C581"
        }
)
foreach ($slot in $expectedSlots) {
    $slotWrites = @(
        $clearWrites |
            Where-Object {
                (
                    [Convert]::ToInt32($_.work_ram_offset, 16) -band 0x7F
                ) -eq $slot
            }
    )
    if ($slotWrites.Count -ne 32) {
        throw (
            "Category-1 slot {0:X2} changed parallel-array clear count."
        ) -f $slot
    }
    for ($array = 0; $array -lt 32; ++$array) {
        if (
            [Convert]::ToInt32(
                $slotWrites[$array].work_ram_offset,
                16
            ) -ne 0x1000 + $array * 0x80 + $slot -or
            $slotWrites[$array].value -ne "00"
        ) {
            throw (
                "Category-1 slot {0:X2} changed clear array {1}."
            ) -f $slot, $array
        }
    }
}

Write-Output "Selective scene semantics verified."
Write-Output (
    "A frame-3000 ball-Y seed reaches the original drain gate and " +
    "category-1 reload at frame 3287."
)
Write-Output (
    "4003 reloads seven persistence slots: " +
    "2F,36,3A,3B,3C,3D,4C."
)
Write-Output (
    "Record writes: 49 direct fields and 224 parallel-array clears."
)
