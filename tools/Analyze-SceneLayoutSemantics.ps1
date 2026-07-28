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

# The common banked-call wrapper maps the requested physical banks into the
# $4000-$7FFF window, calls E262, then restores the previous mapping.
Assert-RomBytes $rom 0x000241 @(
    0xA5, 0x1E, 0x48, 0xA5, 0x1F, 0x48,
    0xA5, 0x30, 0x85, 0x1E, 0x53, 0x04,
    0x1A, 0x85, 0x1F, 0x53, 0x08,
    0x20, 0x62, 0xE2, 0x85, 0x2A,
    0x68, 0x85, 0x1F, 0x53, 0x08,
    0x68, 0x85, 0x1E, 0x53, 0x04, 0x60
) "E241 banked-call wrapper"
Assert-RomBytes $rom 0x000262 @(
    0xA5, 0x2F, 0x8D, 0xBE, 0x2C,
    0xA9, 0x40, 0x8D, 0xBF, 0x2C,
    0xA5, 0x2A, 0x6C, 0xBE, 0x2C
) "E262 banked-call trampoline"

# The full layout mode walks every record. The two dormant modes use the same
# table but call the loader only for category byte 1 or 2.
Assert-RomBytes $rom 0x004088 @(
    0xA9, 0xDB, 0x85, 0x2F, 0xA9, 0x45, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0x11,
    0x20, 0xF6, 0x40,
    0x18, 0xA5, 0x2F, 0x69, 0x0C, 0x85, 0x2F,
    0x62, 0x65, 0x30, 0x85, 0x30, 0x80, 0xE9, 0x60
) "4088 full scene-layout mode"
Assert-RomBytes $rom 0x0040A8 @(
    0xA9, 0xDB, 0x85, 0x2F, 0xA9, 0x45, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0x15,
    0xC9, 0x01, 0xD0, 0x03, 0x20, 0xF6, 0x40
) "40A8 category-1 scene-layout mode"
Assert-RomBytes $rom 0x0040CF @(
    0xA9, 0xDB, 0x85, 0x2F, 0xA9, 0x45, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0xEE,
    0xC9, 0x02, 0xD0, 0x03, 0x20, 0xF6, 0x40
) "40CF category-2 scene-layout mode"

$records = [System.Collections.Generic.List[byte[]]]::new()
$tableOffset = 0x0045DB
while ($rom[$tableOffset] -ne 0xFF) {
    $records.Add([byte[]] $rom[$tableOffset..($tableOffset + 11)])
    $tableOffset += 12
}
if ($records.Count -ne 88 -or $tableOffset -ne 0x0049FB) {
    throw "Normal-table scene layout changed extent."
}

$calls = @(Import-TraceCsv $CallPath)
if ($calls.Count -ne 1) {
    throw "Expected one full scene-layout call, found $($calls.Count)."
}
$call = $calls[0]
if (
    $call.physical_bank -ne "02" -or
    $call.logical_pc -ne "4088" -or
    $call.caller_pc -ne "E252" -or
    $call.return_pc -ne "E255" -or
    $call.return_physical_bank -ne "02" -or
    $call.return_instruction_pc -ne "40A7" -or
    [int] $call.frame -ne 871 -or
    [int64] $call.body_master_cycles -ne 421956 -or
    [int] $call.write_count -ne 11331
) {
    throw "Full scene-layout bounded call changed."
}
if (
    $call.input_mpr2 -ne "02" -or
    $call.output_mpr2 -ne "02" -or
    $call.input_mpr3 -ne "03" -or
    $call.output_mpr3 -ne "03"
) {
    throw "Scene-layout mapped ROM banks changed."
}

$writes = Import-TraceCsv $WritePath
if ($writes.Count -ne 11331) {
    throw "Expected 11,331 scoped scene-layout writes."
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
    if ($fieldWrites.Count -ne 88) {
        throw "Scene-layout field $($field.Pc) changed write count."
    }
    for ($index = 0; $index -lt 88; ++$index) {
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
                "Scene-layout field $($field.Pc) changed record " +
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
    $initializerLowWrites.Count -ne 88 -or
    $initializerHighWrites.Count -ne 88
) {
    throw "Scene-layout initializer scratch writes changed count."
}
for ($index = 0; $index -lt 88; ++$index) {
    if (
        [Convert]::ToInt32($initializerLowWrites[$index].value, 16) -ne
            $records[$index][2] -or
        [Convert]::ToInt32($initializerHighWrites[$index].value, 16) -ne
            $records[$index][3]
    ) {
        throw "Scene-layout initializer changed at record $($index + 1)."
    }
}

# Every record first invokes C574, whose C581 store clears 32 parallel fields.
$clearWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C581"
        }
)
if ($clearWrites.Count -ne 88 * 32) {
    throw "Scene-layout parallel slot clear changed count."
}
$clearIndex = 0
for ($recordIndex = 0; $recordIndex -lt 88; ++$recordIndex) {
    $slot = $records[$recordIndex][1]
    for ($array = 0; $array -lt 32; ++$array) {
        $write = $clearWrites[$clearIndex++]
        $expectedOffset = 0x1000 + $array * 0x80 + $slot
        if (
            [Convert]::ToInt32($write.work_ram_offset, 16) -ne
                $expectedOffset -or
            $write.value -ne "00"
        ) {
            throw (
                "Scene-layout clear changed at record " +
                "$($recordIndex + 1), array $array."
            )
        }
    }
}

$categoryCounts = @{
    0 = 0
    1 = 0
    2 = 0
}
foreach ($record in $records) {
    ++$categoryCounts[[int] $record[0]]
}
if (
    $categoryCounts[0] -ne 64 -or
    $categoryCounts[1] -ne 7 -or
    $categoryCounts[2] -ne 17
) {
    throw "Scene-layout category populations changed."
}

Write-Output "Scene layout semantics verified."
Write-Output (
    "4088: one frame-871 full-layout call initializes 88 records " +
    "in 421956 master cycles."
)
Write-Output (
    "Layout modes: full=active; category-1 and category-2 selective " +
    "entries have separate dynamic regressions."
)
Write-Output (
    "Record writes: 616 direct field writes and 2816 parallel-array clears."
)
Write-Output (
    "The bounded call crosses the frame-871/872 boundary and is not a " +
    "single-frame replay candidate."
)
