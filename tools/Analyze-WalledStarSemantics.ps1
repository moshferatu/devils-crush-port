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

# The fixed upper-left screen-2 record at (32,302) matches the detailed
# guide's brick-walled star. It uses type $82 and initial flags $89.
Assert-RomHex $rom 0x0049A7 @"
00 5B 19 63 20 00 2E 01 03 89 82 00
"@ "Walled-Star layout record"

# Initialization clears wall damage, preserves the layout flags except bits
# 5-6, installs collision selector $10 and completion selector $84, marks the
# wall active, and installs steady dispatch 03:6339.
Assert-RomHex $rom 0x006319 @"
9E 80 39 BD 80 30 29 9F 9D 80 30 A9 10 9D 00 38 A9
84 9D 00 3D BD 00 38 09 40 9D 00 38 20 18 C0
"@ "Walled-Star initialization"

# Four nonterminal collisions each award 50 stored units (5,000 displayed
# points), emit command $2E, and increment the wall-damage appearance. The
# fifth collision takes the terminal branch, disables the wall and submits
# 200 stored units, for a ROM-defined 40,000-point complete-wall total.
Assert-RomHex $rom 0x006339 @"
BD 00 38 10 28 29 7F 9D 00 38 A9 32 85 2F 64 30 20
2A C0 A9 2E 20 87 C0 BD 80 39 C9 04 F0 1E FE 80 39
BD 00 38 09 40 9D 00 38 20 F6 7B AD BC 2D F0 0A BD
80 30 09 60 9D 80 30 80 25 60 BD 80 30 09 60 9D 80
30 A9 C8 85 2F 64 30 20 2A C0 A9 01 20 9F C0 20 18
C0
"@ "Walled-Star hit and terminal states"

# After destruction, the actor waits on the table access fields, clears the
# shared gate latch, runs the ball-routing/animation states, then returns to
# its initializer.
Assert-RomHex $rom 0x00638F @"
AD 53 2D D0 06 AD 32 2D D0 01 60 20 18 C0 AD 53 2D
F0 01 60 9C BC 2D A9 78 20 21 C0 20 18 C0 A9 01 9D
00 3A BD 80 30 29 9F 9D 80 30 20 18 C0 DE 00 3A D0
15 A9 08 9D 00 3A DE 80 39 BD 00 38 09 40 9D 00 38
BD 80 39 F0 01 60 A9 01 20 A2 C0 4C 19 63
"@ "Walled-Star post-destruction access states"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 2651 -or
    $writes.Count -ne 671 -or
    $stateWrites.Count -ne 568
) {
    throw (
        "Expected 2,651 calls, 671 scoped writes, and 568 selected " +
        "Walled-Star state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedReturn = if ($call.logical_pc -eq "6319") {
        "C5E9"
    } else {
        "6375"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("6319", "6339") -or
        $call.input_x -ne "5B" -or
        $call.return_instruction_pc -ne $expectedReturn -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Walled-Star call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Walled-Star call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "6319" = 1
    "6339" = 2650
} "Walled-Star routine"

$steady = @($calls | Where-Object logical_pc -eq "6339")
if (
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Walled-Star steady frame bounds changed."
}
$leafCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 54 -and
            [int] $_.write_count -eq 0
        }
)
if ($leafCalls.Count -ne 2601) {
    throw "Walled-Star no-hit leaf-call population changed."
}
$interruptCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -ne 54 -or
            [int] $_.write_count -ne 0
        }
)
if (
    $interruptCalls.Count -ne 49 -or
    ($interruptCalls | Measure-Object body_master_cycles -Minimum).Minimum -ne
        576 -or
    ($interruptCalls | Measure-Object body_master_cycles -Maximum).Maximum -ne
        1233 -or
    ($interruptCalls | Measure-Object write_count -Minimum).Minimum -ne 10 -or
    ($interruptCalls | Measure-Object write_count -Maximum).Maximum -ne 22
) {
    throw "Walled-Star interrupt-bearing call population changed."
}

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "6319", "6321", "6326", "632B", "6333",
                "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "02:4125:395B:03",
    "02:412B:30DB:89",
    "02:4131:305B:82",
    "03:6319:39DB:00",
    "03:6321:30DB:89",
    "03:6326:385B:10",
    "03:632B:3D5B:84",
    "03:6333:385B:50",
    "01:C5E1:3F5B:39",
    "01:C5E6:3FDB:63"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Walled-Star initial identity changed."
}

Assert-Histogram @(
    $stateWrites |
        Where-Object {
            $_.logical_address -eq "30DB" -and
            $_.instruction_logical_pc -in @("F520", "F54B")
        }
) "instruction_logical_pc" @{
    "F520" = 320
    "F54B" = 199
} "Walled-Star camera-window write"

$hitStateWrites = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -in @(
                "6340", "6358", "6360", "6370", "637B",
                "63A3", "63B0", "63B8", "63BE", "63C5",
                "63C8", "63D0"
            )
        }
)
if ($hitStateWrites.Count -ne 0) {
    throw "The routed Walled-Star hit/access path became active."
}

if (
    [int64] $steady[0].body_master_cycles -ne 54 -or
    [int] $steady[0].write_count -ne 0
) {
    throw "Walled-Star normalized no-hit sample changed."
}

Write-Output "Walled-Star semantics verified."
Write-Output (
    "Fixed slot 5B at (32,302) maps initializer 03:6319 and steady " +
    "dispatch 03:6339 to the upper-left Walled Star."
)
Write-Output (
    "2650 steady calls remain unhit: 2601 are 54-cycle zero-write " +
    "leaves and 49 contain periodic interrupts."
)
Write-Output (
    "ROM-bounded wall damage uses four 50-unit hits with command 2E; " +
    "the fifth collision submits 200 units for 40000 total points."
)
Write-Output (
    "Normalized sample: 03:6339@1, frame 873, 54 cycles, 0 writes."
)
