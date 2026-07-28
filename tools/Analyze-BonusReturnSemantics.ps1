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

# Session bit $80 enters the short presentation/return branch through DD92.
# Unlike the full $10 path, it does not invoke the bank-4 stage-layout loader.
Assert-RomBytes $rom 0x0021A5 @(
    0x89, 0x80, 0xF0, 0x03, 0x4C, 0x0D, 0xC2
) "C1A5 short transition request dispatch"
Assert-RomBytes $rom 0x00220D @(
    0x20, 0x92, 0xDD,
    0xA5, 0x53, 0x29, 0x7F, 0x85, 0x53,
    0x20, 0xBD, 0xFA,
    0xA5, 0x55, 0xD0, 0x03, 0x4C, 0x54, 0xC1,
    0x4C, 0xD0, 0xC1
) "C20D short transition entry"
Assert-RomBytes $rom 0x0021F2 @(
    0x29, 0x02, 0xF0, 0xDA,
    0xA9, 0x03, 0x85, 0x2F,
    0xA9, 0x04, 0x85, 0x30,
    0x20, 0x41, 0xE2,
    0x20, 0x8B, 0xEF,
    0x4C, 0x54, 0xC1
) "C1F2 bonus completion and normal-table return"

# EF8B restores the normal-table session. Export 6 in bank 2 is invoked at
# F052; F05E is a distinct export 0 in bank 8.
Assert-RomBytes $rom 0x00104A @(
    0xA9, 0x06, 0x85, 0x2F,
    0xA9, 0x02, 0x85, 0x30,
    0x20, 0x41, 0xE2,
    0x20, 0xDE, 0xC4,
    0x64, 0x2F, 0xA9, 0x08, 0x85, 0x30,
    0x20, 0x41, 0xE2
) "F04A category-2 and bank-8 calls"
Assert-RomBytes $rom 0x0040CF @(
    0xA9, 0xDB, 0x85, 0x2F, 0xA9, 0x45, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0xEE,
    0xC9, 0x02, 0xD0, 0x03, 0x20, 0xF6, 0x40
) "40CF category-2 selector"

$records = [System.Collections.Generic.List[byte[]]]::new()
$tableOffset = 0x0045DB
while ($rom[$tableOffset] -ne 0xFF) {
    $record = [byte[]] $rom[$tableOffset..($tableOffset + 11)]
    if ($record[0] -eq 2) {
        $records.Add($record)
    }
    $tableOffset += 12
}
if ($records.Count -ne 17) {
    throw "Expected 17 category-2 scene records."
}

$expectedSlots = @(
    0x14, 0x15, 0x2C, 0x32, 0x34, 0x35,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A
)
for ($index = 0; $index -lt $expectedSlots.Count; ++$index) {
    if ($records[$index][1] -ne $expectedSlots[$index]) {
        throw "Category-2 scene slot order changed."
    }
}

$calls = @(Import-TraceCsv $CallPath)
if ($calls.Count -ne 1) {
    throw "Expected one category-2 scene-layout call, found $($calls.Count)."
}
$call = $calls[0]
if (
    $call.physical_bank -ne "02" -or
    $call.logical_pc -ne "4006" -or
    $call.caller_pc -ne "E252" -or
    $call.return_pc -ne "E255" -or
    $call.return_physical_bank -ne "02" -or
    $call.return_instruction_pc -ne "40CE" -or
    [int] $call.frame -ne 1851 -or
    [int64] $call.body_master_cycles -ne 133077 -or
    [int] $call.write_count -ne 3117
) {
    throw "Category-2 bounded call changed."
}
if (
    $call.input_mpr2 -ne "02" -or
    $call.output_mpr2 -ne "02" -or
    $call.input_mpr3 -ne "03" -or
    $call.output_mpr3 -ne "03"
) {
    throw "Category-2 mapped ROM banks changed."
}

$writes = @(Import-TraceCsv $WritePath)
if ($writes.Count -ne 3117) {
    throw "Expected 3,117 scoped category-2 writes."
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
    if ($fieldWrites.Count -ne 17) {
        throw "Category-2 field $($field.Pc) changed write count."
    }
    for ($index = 0; $index -lt 17; ++$index) {
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
                "Category-2 field $($field.Pc) changed record " +
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
    $initializerLowWrites.Count -ne 17 -or
    $initializerHighWrites.Count -ne 17
) {
    throw "Category-2 initializer scratch writes changed count."
}
for ($index = 0; $index -lt 17; ++$index) {
    if (
        [Convert]::ToInt32($initializerLowWrites[$index].value, 16) -ne
            $records[$index][2] -or
        [Convert]::ToInt32($initializerHighWrites[$index].value, 16) -ne
            $records[$index][3]
    ) {
        throw "Category-2 initializer changed at record $($index + 1)."
    }
}

$clearWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C581"
        }
)
if ($clearWrites.Count -ne 17 * 32) {
    throw "Category-2 parallel-array clear changed count."
}
$clearIndex = 0
foreach ($slot in $expectedSlots) {
    for ($array = 0; $array -lt 32; ++$array) {
        $write = $clearWrites[$clearIndex++]
        if (
            [Convert]::ToInt32($write.work_ram_offset, 16) -ne
                0x1000 + $array * 0x80 + $slot -or
            $write.value -ne "00"
        ) {
            throw (
                "Category-2 slot {0:X2} changed clear array {1}."
            ) -f $slot, $array
        }
    }
}

Write-Output "Bonus-return semantics verified."
Write-Output (
    "A frame-1300 short transition returns through EF8B and reaches " +
    "category-2 reload at frame 1851."
)
Write-Output (
    "4006 reloads 17 respawn slots: " +
    "14,15,2C,32,34,35,50-5A."
)
Write-Output (
    "Record writes: 119 direct fields and 544 parallel-array clears."
)
