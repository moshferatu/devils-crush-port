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
    [string] $StateWritePath
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

    [byte[]] $expected = @(
        $Hex.Split(
            [char[]] @(" ", "`r", "`n", "`t"),
            [System.StringSplitOptions]::RemoveEmptyEntries
        ) | ForEach-Object { [Convert]::ToByte($_, 16) }
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

# This type-$82 surface sits directly above and left of the Tower and its
# insect source. Its gate-dependent completion selector is the manual's
# Tower Guard / bonus-stage-5 entrance contract.
Assert-RomHex $rom 0x00478B @"
00 2E 99 60 84 00 FE 01 37 84 82 00
"@ "Tower-Guard layout record"

# Initialization installs collision selector $10, closed completion selector
# $84, and steady dispatch 03:60A6.
Assert-RomHex $rom 0x006099 @"
A9 10 9D 00 38 A9 84 9D 00 3D 20 18 C0
"@ "Tower-Guard initialization"

# The live surface remains closed while Tower gate counter $2D4D is nonzero.
# Counter zero changes its completion selector from $84 to $8E.
Assert-RomHex $rom 0x0060A6 @"
A9 84 AC 4D 2D D0 02 A9 8E 9D 00 3D 60
"@ "Tower-Guard selector update"

# The adjacent Tower owns the five-step counter. Its first routed hit arms
# $2CFC; subsequent qualifying passes decrement $2D4D until the Guard opens.
Assert-RomHex $rom 0x0060B3 @"
9C 3F 2D A9 03 9D 00 3B A9 05 8D 4D 2D 20 18 C0
"@ "Tower gate-counter initialization"
Assert-RomHex $rom 0x0060F9 @"
AD FC 2C F0 14 CE 4D 2D F0 28 DE 00 3B D0 22 A9 03
9D 00 3B 20 69 C0 80 05 A9 01 8D FC 2C
"@ "Tower gate-counter progression"

# Tower slot $2D reaches this prefix when its qualifying latch is set and the
# decrement of the five-step gate reaches zero. It clears Tower collision
# state and schedules the later collision-gated stage-selection continuation.
Assert-RomHex $rom 0x00612B @"
BD 00 38 29 7F 9D 00 38 20 18 C0 BD 00 38 30 01 60
A9 01 8D 80 3C A9 10 20 21 C0 20 18 C0 A9 05 85 55
A5 53 09 10 85 53 A9 41 20 87 C0 A9 05 8D 4D 2D A9
01 8D 2D 2D 20 18 C0
"@ "Tower-Guard open entrance"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 2651 -or
    $writes.Count -ne 3412 -or
    $stateWrites.Count -ne 3230
) {
    throw (
        "Expected 2,651 calls, 3,412 scoped writes, and 3,230 selected " +
        "Tower-Guard state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedReturn = if ($call.logical_pc -eq "6099") {
        "C5E9"
    } else {
        "60B2"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("6099", "60A6") -or
        $call.input_x -ne "2E" -or
        $call.return_instruction_pc -ne $expectedReturn -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Tower-Guard call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Tower-Guard call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "6099" = 1
    "60A6" = 2650
} "Tower-Guard routine"

$initializer = @($calls | Where-Object logical_pc -eq "6099")
if (
    [int] $initializer[0].frame -ne 872 -or
    [int64] $initializer[0].body_master_cycles -ne 234 -or
    [int] $initializer[0].write_count -ne 9
) {
    throw "Tower-Guard initializer call changed."
}

$steady = @($calls | Where-Object logical_pc -eq "60A6")
if (
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Tower-Guard steady frame bounds changed."
}
$closedLeaves = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 48 -and
            [int] $_.write_count -eq 1
        }
)
if ($closedLeaves.Count -ne 2592) {
    throw "Tower-Guard closed-leaf population changed."
}
$interruptCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -ne 48 -or
            [int] $_.write_count -ne 1
        }
)
if (
    $interruptCalls.Count -ne 58 -or
    ($interruptCalls | Measure-Object body_master_cycles -Minimum).Minimum -ne
        570 -or
    ($interruptCalls | Measure-Object body_master_cycles -Maximum).Maximum -ne
        1152 -or
    ($interruptCalls | Measure-Object write_count -Minimum).Minimum -ne 11 -or
    ($interruptCalls | Measure-Object write_count -Maximum).Maximum -ne 19
) {
    throw "Tower-Guard interrupt-call population changed."
}

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "609B", "60A0", "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "02:4125:392E:37",
    "02:412B:30AE:84",
    "02:4131:302E:82",
    "03:609B:382E:10",
    "03:60A0:3D2E:84",
    "01:C5E1:3F2E:A6",
    "01:C5E6:3FAE:60"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Tower-Guard initial identity changed."
}

$selectorWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "60AF" -and
            $_.work_ram_offset -eq "1D2E"
        }
)
if (
    $selectorWrites.Count -ne 2650 -or
    @($selectorWrites | Where-Object value -ne "84").Count -ne 0
) {
    throw "Tower-Guard routed selector values changed."
}

$gateTimeline = @(
    $stateWrites |
        Where-Object {
            $_.logical_address -in @("2CFC", "2D2D", "2D4D")
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedGateTimeline = @(
    "1:01:C100:2CFC:00",
    "1:01:C100:2D2D:00",
    "1:01:C100:2D4D:00",
    "872:03:60BD:2D4D:05",
    "941:02:55E6:2CFC:00",
    "3353:03:6114:2CFC:01"
)
if (($gateTimeline -join ",") -ne ($expectedGateTimeline -join ",")) {
    throw "Tower-Guard gate timeline changed."
}

if (
    [int64] $steady[0].body_master_cycles -ne 48 -or
    [int] $steady[0].write_count -ne 1
) {
    throw "Tower-Guard normalized closed sample changed."
}

Write-Output "Tower-Guard semantics verified."
Write-Output (
    "Fixed slot 2E at (132,510) maps initializer 03:6099 and steady " +
    "dispatch 03:60A6 to the Tower Guard / entrance surface."
)
Write-Output (
    "2650 updates keep selector 84: 2592 are 48-cycle one-write leaves " +
    "and 58 contain periodic interrupts."
)
Write-Output (
    "ROM-bounded gate zero selects 8E; Tower terminal 612B schedules the " +
    "later collision-gated BS-5 continuation."
)
Write-Output (
    "Normalized sample: 03:60A6@1, frame 873, 48 cycles, 1 write."
)
