# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

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

function Assert-Counts {
    param(
        [hashtable] $Actual,
        [hashtable] $Expected,
        [string] $Label
    )

    if ($Actual.Count -ne $Expected.Count) {
        throw "$Label key set changed."
    }
    foreach ($key in $Expected.Keys) {
        if (
            -not $Actual.ContainsKey($key) -or
            $Actual[$key] -ne $Expected[$key]
        ) {
            throw "$Label count changed for $key."
        }
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# C575 swaps the requested slot into Y and clears 32 parallel arrays spaced
# 0x80 bytes apart, beginning with the occupancy/type byte at $3000,Y.
Assert-RomBytes $rom 0x002574 @(
    0x5A, 0xA0, 0x20, 0x02,
    0xA9, 0x00, 0x85, 0x2D, 0xA9, 0x30, 0x85, 0x2E,
    0x62, 0x91, 0x2D,
    0x18, 0xA5, 0x2D, 0x69, 0x80, 0x85, 0x2D,
    0x62, 0x65, 0x2E, 0x85, 0x2E,
    0xCA, 0xD0, 0xEE, 0x02, 0x7A, 0x60
) "C575 parallel actor-slot clear"

# C5AB scans the generic pool from slot 04 through 7B and claims the first
# empty slot with a provisional occupied value.
Assert-RomBytes $rom 0x0025AB @(
    0xA0, 0x04, 0xB9, 0x00, 0x30, 0xF0, 0x07,
    0xC8, 0xC0, 0x7C, 0xD0, 0xF6, 0x38, 0x60,
    0xA9, 0x80, 0x99, 0x00, 0x30,
    0xA5, 0x55, 0xF0, 0x05,
    0xA9, 0xC0, 0x99, 0x00, 0x30,
    0x18, 0x60
) "C5AB generic actor-slot allocator"

# The layout loop starts at ROM table $45DB. Each 12-byte record selects a
# slot, supplies an initializer entry, integer position, state, flags, type,
# and one accumulator parameter for that initializer.
Assert-RomBytes $rom 0x004088 @(
    0xA9, 0xDB, 0x85, 0x2F, 0xA9, 0x45, 0x85, 0x30,
    0xB2, 0x2F, 0xC9, 0xFF, 0xF0, 0x11,
    0x20, 0xF6, 0x40,
    0x18, 0xA5, 0x2F, 0x69, 0x0C, 0x85, 0x2F,
    0x62, 0x65, 0x30, 0x85, 0x30, 0x80, 0xE9, 0x60
) "4088 static actor-layout loop"
Assert-RomBytes $rom 0x0040F6 @(
    0xA0, 0x01, 0xB1, 0x2F, 0xAA, 0x20, 0x27, 0xC0,
    0xC8, 0xB1, 0x2F, 0x8D, 0xBE, 0x2C,
    0xC8, 0xB1, 0x2F, 0x8D, 0xBF, 0x2C,
    0xC8, 0xB1, 0x2F, 0x9D, 0x80, 0x31,
    0xC8, 0xB1, 0x2F, 0x9D, 0x00, 0x32,
    0xC8, 0xB1, 0x2F, 0x9D, 0x00, 0x33,
    0xC8, 0xB1, 0x2F, 0x9D, 0x80, 0x33,
    0xC8, 0xB1, 0x2F, 0x9D, 0x00, 0x39,
    0xC8, 0xB1, 0x2F, 0x9D, 0x80, 0x30,
    0xC8, 0xB1, 0x2F, 0x9D, 0x00, 0x30,
    0xC8, 0xB1, 0x2F, 0x6C, 0xBE, 0x2C
) "40F6 static actor-record loader"

$records = [System.Collections.Generic.List[object]]::new()
$tableOffset = 0x0045DB
while ($rom[$tableOffset] -ne 0xFF) {
    if ($tableOffset + 11 -ge $rom.Count) {
        throw "Static actor table has no terminator."
    }
    $record = $rom[$tableOffset..($tableOffset + 11)]
    $records.Add([pscustomobject] @{
        Category = [int] $record[0]
        Slot = [int] $record[1]
        Initializer = ([int] $record[3] -shl 8) -bor $record[2]
        X = ([int] $record[5] -shl 8) -bor $record[4]
        Y = ([int] $record[7] -shl 8) -bor $record[6]
        State = [int] $record[8]
        Flags = [int] $record[9]
        Type = [int] $record[10]
        Parameter = [int] $record[11]
    })
    $tableOffset += 12
}
if ($records.Count -ne 88 -or $tableOffset -ne 0x0049FB) {
    throw "Static actor-layout extent changed."
}
for ($index = 0; $index -lt $records.Count; ++$index) {
    if ($records[$index].Slot -ne 0x0A + $index) {
        throw "Static actor-layout slot order changed at record $index."
    }
}

$categoryCounts = @{}
$typeCounts = @{}
$initializers = @{}
foreach ($record in $records) {
    foreach ($pair in @(
        @($categoryCounts, [string] $record.Category),
        @($typeCounts, ("{0:X2}" -f $record.Type)),
        @($initializers, ("{0:X4}" -f $record.Initializer))
    )) {
        if (-not $pair[0].ContainsKey($pair[1])) {
            $pair[0][$pair[1]] = 0
        }
        ++$pair[0][$pair[1]]
    }
}
Assert-Counts $categoryCounts @{
    "0" = 64
    "1" = 7
    "2" = 17
} "Static actor category"
Assert-Counts $typeCounts @{
    "80" = 12
    "81" = 1
    "82" = 44
    "83" = 19
    "84" = 12
} "Static actor type"
if ($initializers.Count -ne 59) {
    throw "Expected 59 distinct static actor initializers."
}

$writes = Import-TraceCsv $WritePath
if ($writes.Count -ne 3872) {
    throw "Expected 3,872 actor identity writes, found $($writes.Count)."
}
$typeWrites = @(
    $writes |
        Where-Object {
            $address = [Convert]::ToInt32($_.logical_address, 16)
            $address -ge 0x3000 -and $address -le 0x307F
        }
)
$dispatchWrites = @(
    $writes |
        Where-Object {
            $address = [Convert]::ToInt32($_.logical_address, 16)
            $address -ge 0x3F00 -and $address -le 0x3FFF
        }
)
if ($typeWrites.Count -ne 576 -or $dispatchWrites.Count -ne 3296) {
    throw "Actor type/dispatch write partition changed."
}

$typeWriterCounts = @{}
foreach ($write in $typeWrites) {
    $key = (
        "$($write.instruction_physical_bank):" +
        "$($write.instruction_logical_pc)"
    )
    if (-not $typeWriterCounts.ContainsKey($key)) {
        $typeWriterCounts[$key] = 0
    }
    ++$typeWriterCounts[$key]
}
Assert-Counts $typeWriterCounts @{
    "01:C100" = 128
    "08:5857" = 1
    "08:585A" = 127
    "01:C581" = 132
    "01:C65D" = 1
    "01:D335" = 3
    "01:D376" = 3
    "02:4131" = 88
    "01:C5BB" = 44
    "03:7E8F" = 10
    "02:5F80" = 4
    "03:63F9" = 18
    "03:6465" = 2
    "02:425A" = 7
    "02:43B5" = 1
    "08:5685" = 1
    "08:5915" = 1
    "08:5A30" = 3
    "09:6FB5" = 1
    "09:70F2" = 1
} "Actor type writer"

$layoutWrites = @(
    $typeWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4131"
        }
)
for ($index = 0; $index -lt $records.Count; ++$index) {
    $write = $layoutWrites[$index]
    if (
        [Convert]::ToInt32($write.logical_address, 16) -ne
            0x3000 + $records[$index].Slot -or
        [Convert]::ToInt32($write.value, 16) -ne
            $records[$index].Type
    ) {
        throw "Static layout write $($index + 1) changed."
    }
}

$ballWrite = @(
    $typeWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C65D"
        }
)
if (
    $ballWrite.Count -ne 1 -or
    $ballWrite[0].logical_address -ne "3000" -or
    $ballWrite[0].value -ne "80"
) {
    throw "Slot-0 ball allocation changed."
}

$leftControlSlots = @(
    $typeWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "D335"
        } |
        ForEach-Object {
            [Convert]::ToInt32($_.logical_address, 16) - 0x3000
        }
)
$rightControlSlots = @(
    $typeWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "D376"
        } |
        ForEach-Object {
            [Convert]::ToInt32($_.logical_address, 16) - 0x3000
        }
)
if (
    ($leftControlSlots -join ",") -ne "4,6,8" -or
    ($rightControlSlots -join ",") -ne "5,7,9"
) {
    throw "Paired control-piece slot ownership changed."
}

$allocations = @(
    $typeWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5BB"
        }
)
if ($allocations.Count -ne 44) {
    throw "Expected 44 generic dynamic actor allocations."
}
foreach ($write in $allocations) {
    $slot = [Convert]::ToInt32($write.logical_address, 16) - 0x3000
    if ($slot -lt 0x62 -or $slot -gt 0x71 -or $write.value -ne "80") {
        throw "Generic dynamic allocation left slots 62-71."
    }
}
$dynamicFrees = @(
    $typeWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C581" -and
            (
                [Convert]::ToInt32($_.logical_address, 16) - 0x3000
            ) -ge 0x62 -and
            (
                [Convert]::ToInt32($_.logical_address, 16) - 0x3000
            ) -le 0x71
        }
)
if ($dynamicFrees.Count -ne 35) {
    throw "Expected 35 dynamic actor-slot frees."
}

$dispatchLowCount = @(
    $dispatchWrites |
        Where-Object {
            $address = [Convert]::ToInt32($_.logical_address, 16)
            $address -ge 0x3F00 -and $address -le 0x3F7F
        }
).Count
$dispatchHighCount = @(
    $dispatchWrites |
        Where-Object {
            $address = [Convert]::ToInt32($_.logical_address, 16)
            $address -ge 0x3F80 -and $address -le 0x3FFF
        }
).Count
if ($dispatchLowCount -ne 1648 -or $dispatchHighCount -ne 1648) {
    throw "Actor dispatch pointer halves are unbalanced."
}

$expectedDynamicDispatchWriters = @{
    "03:7EE1:FA" = 10
    "03:7EE6:7E" = 10
    "02:5FC9:DB" = 4
    "02:5FCE:5F" = 4
    "02:425F:90" = 7
    "02:4264:42" = 7
    "02:42A8:B0" = 7
    "02:42AD:42" = 7
    "02:43AB:1E" = 1
    "02:43B0:44" = 1
    "03:7C2B:34" = 19
    "03:7C30:7C" = 19
    "03:7C57:6F" = 3
    "03:7C5C:7C" = 3
    "03:7F8C:81" = 8
    "03:7F91:7F" = 8
}
$actualDynamicDispatchWriters = @{}
foreach ($write in $dispatchWrites) {
    $key = (
        "$($write.instruction_physical_bank):" +
        "$($write.instruction_logical_pc):$($write.value)"
    )
    if ($expectedDynamicDispatchWriters.ContainsKey($key)) {
        if (-not $actualDynamicDispatchWriters.ContainsKey($key)) {
            $actualDynamicDispatchWriters[$key] = 0
        }
        ++$actualDynamicDispatchWriters[$key]
    }
}
Assert-Counts `
    $actualDynamicDispatchWriters `
    $expectedDynamicDispatchWriters `
    "Dynamic dispatch writer"

$commonLowTransitions = @(
    $dispatchWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1"
        }
).Count
$commonHighTransitions = @(
    $dispatchWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6"
        }
).Count
if ($commonLowTransitions -ne 1107 -or $commonHighTransitions -ne 1107) {
    throw "Common actor dispatch transitions changed."
}

Write-Output "Actor layout semantics verified."
Write-Output (
    "Static layout: 88 records cover slots 0A-61 with " +
    "59 initializer entrypoints."
)
Write-Output (
    "Static types: 80=12, 81=1, 82=44, 83=19, 84=12; " +
    "categories 0=64, 1=7, 2=17."
)
Write-Output (
    "Dynamic pool: 44 allocations and 35 frees use slots 62-71."
)
Write-Output (
    "Dispatch state: 3296 writes, 7 observed dynamic writer families, " +
    "1107 common pointer transitions."
)
