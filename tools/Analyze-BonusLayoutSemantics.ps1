# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 6)]
    [int] $Stage,

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

# Session request bit $10 selects the full bonus initializer at ED86. That
# path preserves normal-table state and invokes export 0 in physical bank 4.
Assert-RomBytes $rom 0x002198 @(
    0xA5, 0x53, 0x89, 0x10, 0xD0, 0x1C
) "C198 full bonus-stage request"
Assert-RomBytes $rom 0x0021C2 @(
    0xA9, 0x04, 0x85, 0x1E, 0x53, 0x04,
    0x1A, 0x85, 0x1F, 0x53, 0x08,
    0x20, 0x86, 0xED
) "C1C2 full bonus-stage initializer call"
Assert-RomBytes $rom 0x000EAD @(
    0xA9, 0x33, 0x85, 0x2F,
    0xA9, 0x08, 0x85, 0x30, 0x20, 0x41, 0xE2,
    0x64, 0x2F,
    0xA9, 0x04, 0x85, 0x30, 0x20, 0x41, 0xE2,
    0x20, 0x39, 0xC5
) "EEAD bonus-stage layout request"

# Bank 4 export 0 selects one of six record tables by $55. Each 12-byte
# record is dynamically allocated, initialized, and dispatched immediately.
Assert-RomBytes $rom 0x008000 @(
    0x4C, 0x06, 0x40,
    0x4C, 0x6C, 0x45,
    0xA5, 0x55, 0x3A, 0x0A, 0xA8,
    0xB9, 0x85, 0x40, 0x85, 0x2F,
    0xB9, 0x86, 0x40, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0x11,
    0x20, 0x2D, 0x40
) "4000 bonus-stage layout selector"

$expectedPointers = @(0x4093, 0x4154, 0x41B5, 0x4246, 0x4337, 0x4458)
$expectedCounts = @(16, 8, 12, 20, 24, 7)
$expectedFrames = @(1341, 1342, 1342, 1342, 1342, 1342)
$expectedCycles = @(76704, 33240, 52353, 91932, 114144, 28734)
$expectedWrites = @(590, 305, 422, 600, 716, 278)

$recordsByStage = [System.Collections.Generic.List[object]]::new()
for ($stageIndex = 0; $stageIndex -lt 6; ++$stageIndex) {
    $pointerOffset = 0x008085 + $stageIndex * 2
    $pointer = (
        [int] $rom[$pointerOffset] -bor
        ([int] $rom[$pointerOffset + 1] -shl 8)
    )
    if ($pointer -ne $expectedPointers[$stageIndex]) {
        throw "Bonus-stage $($stageIndex + 1) table pointer changed."
    }

    $records = [System.Collections.Generic.List[byte[]]]::new()
    $tableOffset = 0x008000 + ($pointer -band 0x1FFF)
    while ($rom[$tableOffset] -ne 0xFF) {
        $records.Add([byte[]] $rom[$tableOffset..($tableOffset + 11)])
        $tableOffset += 12
    }
    if ($records.Count -ne $expectedCounts[$stageIndex]) {
        throw "Bonus-stage $($stageIndex + 1) record count changed."
    }
    $recordsByStage.Add($records)
}

$calls = @(Import-TraceCsv $CallPath)
if ($calls.Count -ne 1) {
    throw "Expected one BS-$Stage layout call, found $($calls.Count)."
}
$call = $calls[0]
$stageOffset = $Stage - 1
if (
    $call.physical_bank -ne "04" -or
    $call.logical_pc -ne "4000" -or
    $call.caller_pc -ne "E252" -or
    $call.return_pc -ne "E255" -or
    $call.return_physical_bank -ne "04" -or
    $call.return_instruction_pc -ne "402C" -or
    [int] $call.frame -ne $expectedFrames[$stageOffset] -or
    [int64] $call.body_master_cycles -ne $expectedCycles[$stageOffset] -or
    [int] $call.write_count -ne $expectedWrites[$stageOffset]
) {
    throw "BS-$Stage bounded layout call changed."
}
if (
    $call.input_mpr2 -ne "04" -or
    $call.output_mpr2 -ne "04" -or
    $call.input_mpr3 -ne "05" -or
    $call.output_mpr3 -ne "05"
) {
    throw "BS-$Stage mapped ROM banks changed."
}

$writes = @(Import-TraceCsv $WritePath)
if ($writes.Count -ne $expectedWrites[$stageOffset]) {
    throw "BS-$Stage scoped write count changed."
}
$records = $recordsByStage[$stageOffset]
$recordCount = $records.Count

$allocations = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "4080"
        }
)
if ($allocations.Count -ne $recordCount) {
    throw "BS-$Stage dynamic allocation count changed."
}
$slots = @(
    $allocations |
        ForEach-Object {
            [Convert]::ToInt32($_.work_ram_offset, 16) - 0x1000
        }
)
if (@($slots | Where-Object { $_ -lt 4 -or $_ -gt 0x7B }).Count -ne 0) {
    throw "BS-$Stage allocated outside the legal dynamic range."
}

$pointerLowWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "4035"
        }
)
$pointerHighWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "403B"
        }
)
if (
    $pointerLowWrites.Count -ne $recordCount -or
    $pointerHighWrites.Count -ne $recordCount
) {
    throw "BS-$Stage initializer scratch writes changed count."
}
for ($index = 0; $index -lt $recordCount; ++$index) {
    if (
        [Convert]::ToInt32($pointerLowWrites[$index].value, 16) -ne
            $records[$index][2] -or
        [Convert]::ToInt32($pointerHighWrites[$index].value, 16) -ne
            $records[$index][3]
    ) {
        throw "BS-$Stage initializer changed at record $($index + 1)."
    }
}

$directFields = @(
    [pscustomobject] @{ Pc = "4041"; Base = 0x1180; Byte = 4 },
    [pscustomobject] @{ Pc = "4047"; Base = 0x1200; Byte = 5 },
    [pscustomobject] @{ Pc = "404D"; Base = 0x1300; Byte = 6 },
    [pscustomobject] @{ Pc = "4053"; Base = 0x1380; Byte = 7 },
    [pscustomobject] @{ Pc = "4059"; Base = 0x1900; Byte = 8 },
    [pscustomobject] @{ Pc = "405F"; Base = 0x1080; Byte = 9 },
    [pscustomobject] @{ Pc = "4067"; Base = 0x1000; Byte = 10 }
)
foreach ($field in $directFields) {
    $fieldWrites = @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -eq "04" -and
                $_.instruction_logical_pc -eq $field.Pc
            }
    )
    if ($fieldWrites.Count -ne $recordCount) {
        throw "BS-$Stage field $($field.Pc) changed write count."
    }
    for ($index = 0; $index -lt $recordCount; ++$index) {
        $expectedValue = [int] $records[$index][$field.Byte]
        if ($field.Pc -eq "4067") {
            $expectedValue = $expectedValue -bor 0x40
        }
        if (
            [Convert]::ToInt32(
                $fieldWrites[$index].work_ram_offset,
                16
            ) -ne $field.Base + $slots[$index] -or
            [Convert]::ToInt32(
                $fieldWrites[$index].value,
                16
            ) -ne $expectedValue
        ) {
            throw (
                "BS-$Stage field $($field.Pc) changed record " +
                "$($index + 1)."
            )
        }
    }
}

Write-Output "Bonus layout semantics verified for BS-$Stage."
Write-Output (
    "BS-${Stage}: $recordCount records, " +
    "$($expectedCycles[$stageOffset]) master cycles, " +
    "$($expectedWrites[$stageOffset]) scoped writes."
)
Write-Output (
    "All stage populations: BS-1=16, BS-2=8, BS-3=12, " +
    "BS-4=20, BS-5=24, BS-6=7."
)
