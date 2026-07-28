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

function Assert-Call {
    param(
        [object] $Call,
        [int] $Sequence,
        [int] $Frame,
        [string] $Pc,
        [string] $Slot,
        [int64] $Cycles,
        [int] $Writes,
        [string] $Label
    )

    if (
        [int] $Call.sequence -ne $Sequence -or
        [int] $Call.frame -ne $Frame -or
        $Call.physical_bank -ne "02" -or
        $Call.logical_pc -ne $Pc -or
        $Call.input_x -ne $Slot -or
        $Call.return_instruction_pc -ne "4CF1" -or
        [int64] $Call.body_master_cycles -ne $Cycles -or
        [int] $Call.write_count -ne $Writes
    ) {
        throw "$Label changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# These type-$84 records sit at the lower-left and upper-right of screen 2,
# matching the detailed guide's two Side Stars and their 8,000-point access.
Assert-RomHex $rom 0x004623 @"
00 10 54 4A 26 00 74 01 43 82 84 00
"@ "Lower-left Side-Star access layout record"
Assert-RomHex $rom 0x00463B @"
00 12 4C 4A EE 00 34 01 16 82 84 00
"@ "Upper-right Side-Star access layout record"

# The initializers select different appearance records, then install mirrored
# steady entrypoints through the same animation state.
Assert-RomHex $rom 0x004A4C @"
A9 0A 9D 80 3C 4C F1 4B
"@ "Upper-right Side-Star initializer"
Assert-RomHex $rom 0x004A54 @"
A9 1C 9D 80 3C 4C BC 4B
"@ "Lower-left Side-Star initializer"
Assert-RomHex $rom 0x004BBC @"
A9 01 9D 00 3A A9 BC 9D 00 3E A9 4B 9D 80 3E
A9 D5 9D 00 3F A9 4B 9D 80 3F
"@ "Lower-left Side-Star dispatch setup"
Assert-RomHex $rom 0x004BF1 @"
A9 01 9D 00 3A A9 F1 9D 00 3E A9 4B 9D 80 3E
A9 0A 9D 00 3F A9 4C 9D 80 3F
"@ "Upper-right Side-Star dispatch setup"

# A routed collision would award $50 binary units (80, or 8,000 displayed
# points). The lower-left access always sets Giant-Face transformation request
# bit 1. The upper-right access does so when a face form is already persisted.
Assert-RomHex $rom 0x004BD5 @"
BD 00 38 30 03 4C DA 4C A9 50 85 2F 64 30 20 2A C0
AD 0C 2D 09 02 8D 0C 2D 4C FB 4C
"@ "Lower-left Side-Star hit"
Assert-RomHex $rom 0x004C0A @"
BD 00 38 30 03 4C DA 4C A9 50 85 2F 64 30 20 2A C0
AD 58 22 F0 03 4C 6D 4C
"@ "Upper-right Side-Star hit"
Assert-RomHex $rom 0x004C6D @"
AD 0C 2D 09 02 8D 0C 2D 4C FB 4C
"@ "Upper-right Giant-Face transformation request"

# With persisted form zero, the upper-right surface instead runs a six-step
# entrance animation, selects bonus stage 6 in $55, and marks the session
# transition in $53 before entering the shared completion path.
Assert-RomHex $rom 0x004C23 @"
A9 04 9D 00 3A 20 6F 40 A9 04 8D 80 39 20 18 C0
AD 80 39 C9 06 F0 0E DE 00 3A D0 08 A9 04 9D 00 3A
EE 80 39 60 AD 80 30 09 20 8D 80 30 A9 06 85 55
A5 53 09 10 85 53 20 18 C0 A9 06 8D 80 39
AD 80 3C 09 01 8D 80 3C 4C 20 4D
"@ "Upper-right bonus-stage-6 entrance"

# No collision occurs on the canonical route. Both live routines use this
# four-tick, four-appearance idle loop.
Assert-RomHex $rom 0x004CDA @"
DE 00 3A D0 12 A9 04 9D 00 3A FE 80 39 BD 80 39
C9 03 90 03 9E 80 39 60
"@ "Shared Side-Star idle animation"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 2184 -or
    $writes.Count -ne 4298 -or
    $stateWrites.Count -ne 5246
) {
    throw (
        "Expected 2,184 calls, 4,298 scoped writes, and 5,246 selected " +
        "Side-Star state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedSlot = if ($call.logical_pc -in @("4A54", "4BD5")) {
        "10"
    } else {
        "12"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("4A54", "4BD5", "4A4C", "4C0A") -or
        $call.input_x -ne $expectedSlot -or
        $call.return_instruction_pc -ne "4CF1" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Side-Star call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Side-Star call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "4A54" = 1
    "4BD5" = 1277
    "4A4C" = 1
    "4C0A" = 905
} "Side-Star routine"

Assert-Call $calls[0] 1 871 "4A54" "10" 273 9 (
    "Lower-left Side-Star initializer"
)
Assert-Call $calls[1] 2 871 "4BD5" "10" 135 3 (
    "Lower-left Side-Star normalized sample"
)
Assert-Call $calls[2] 3 871 "4A4C" "12" 273 9 (
    "Upper-right Side-Star initializer"
)
Assert-Call $calls[3] 4 871 "4C0A" "12" 135 3 (
    "Upper-right Side-Star normalized sample"
)

$left = @($calls | Where-Object logical_pc -eq "4BD5")
$right = @($calls | Where-Object logical_pc -eq "4C0A")
if (
    [int] $left[0].frame -ne 871 -or
    [int] $left[-1].frame -ne 3600 -or
    [int] $right[0].frame -ne 871 -or
    [int] $right[-1].frame -ne 3600
) {
    throw "Side-Star steady frame bounds changed."
}

function Assert-IdlePopulation {
    param(
        [object[]] $Rows,
        [int] $ExpectedShort,
        [int] $ExpectedAdvance,
        [int] $ExpectedWrap,
        [int] $ExpectedInterrupts,
        [int64] $MinimumInterruptCycles,
        [int64] $MaximumInterruptCycles,
        [int] $MinimumInterruptWrites,
        [int] $MaximumInterruptWrites,
        [string] $Label
    )

    $short = @(
        $Rows |
            Where-Object {
                [int64] $_.body_master_cycles -eq 66 -and
                [int] $_.write_count -eq 1
            }
    )
    $advance = @(
        $Rows |
            Where-Object {
                [int64] $_.body_master_cycles -eq 135 -and
                [int] $_.write_count -eq 3
            }
    )
    $wrap = @(
        $Rows |
            Where-Object {
                [int64] $_.body_master_cycles -eq 144 -and
                [int] $_.write_count -eq 4
            }
    )
    $interrupts = @(
        $Rows |
            Where-Object {
                [int64] $_.body_master_cycles -notin @(66, 135, 144)
            }
    )
    if (
        $short.Count -ne $ExpectedShort -or
        $advance.Count -ne $ExpectedAdvance -or
        $wrap.Count -ne $ExpectedWrap -or
        $interrupts.Count -ne $ExpectedInterrupts -or
        ($interrupts | Measure-Object body_master_cycles -Minimum).Minimum -ne
            $MinimumInterruptCycles -or
        ($interrupts | Measure-Object body_master_cycles -Maximum).Maximum -ne
            $MaximumInterruptCycles -or
        ($interrupts | Measure-Object write_count -Minimum).Minimum -ne
            $MinimumInterruptWrites -or
        ($interrupts | Measure-Object write_count -Maximum).Maximum -ne
            $MaximumInterruptWrites
    ) {
        throw "$Label idle-call population changed."
    }
}

Assert-IdlePopulation $left 934 202 103 38 588 951 11 17 "Lower-left"
Assert-IdlePopulation $right 660 148 70 27 588 1245 11 23 "Upper-right"

$initializerState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 871 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "4A56", "4BBE", "4BC3", "4BC8", "4BCD", "4BD2",
                "4A4E", "4BF3", "4BF8", "4BFD", "4C02", "4C07"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitializerState = @(
    "02:4125:3910:43",
    "02:412B:3090:82",
    "02:4131:3010:84",
    "02:4A56:3C90:1C",
    "02:4BBE:3A10:01",
    "02:4BC3:3E10:BC",
    "02:4BC8:3E90:4B",
    "02:4BCD:3F10:D5",
    "02:4BD2:3F90:4B",
    "02:4125:3912:16",
    "02:412B:3092:82",
    "02:4131:3012:84",
    "02:4A4E:3C92:0A",
    "02:4BF3:3A12:01",
    "02:4BF8:3E12:F1",
    "02:4BFD:3E92:4B",
    "02:4C02:3F12:0A",
    "02:4C07:3F92:4C"
)
if (($initializerState -join ",") -ne ($expectedInitializerState -join ",")) {
    throw "Side-Star initializer state changed."
}

$hitWrites = @(
    $stateWrites |
        Where-Object {
            (
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -in @("4BEC", "4C73")
            ) -or (
                $_.logical_address -eq "2D0C" -and
                $_.instruction_logical_pc -notin @("C100", "74CC")
            )
        }
)
if ($hitWrites.Count -ne 0) {
    throw "A Side-Star hit unexpectedly occurred on the canonical route."
}

$globalTimeline = @(
    $stateWrites |
        Where-Object {
            $_.logical_address -in @("2258", "2D0C", "2D16", "2D17")
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedGlobalTimeline = @(
    "1:01:C100:2258:00",
    "1:01:C100:2D0C:00",
    "1:01:C100:2D16:00",
    "1:01:C100:2D17:00",
    "1:00:F8F4:2258:00",
    "872:03:7D5A:2D16:00",
    "872:03:7DF8:2D17:00",
    "873:03:7EF6:2D16:01",
    "873:03:7EF6:2D17:01",
    "941:03:74C6:2258:00",
    "941:03:74CC:2D0C:00",
    "988:03:7EF6:2D16:02",
    "988:03:7EF6:2D17:02",
    "1036:03:7EF6:2D16:03",
    "1036:03:7EF6:2D17:03",
    "1084:03:7EF6:2D16:04",
    "1084:03:7EF6:2D17:04",
    "1132:03:7EF6:2D16:05",
    "1132:03:7EF6:2D17:05",
    "1852:03:7FAB:2D16:04",
    "1970:03:7FAB:2D17:04"
)
if (($globalTimeline -join ",") -ne ($expectedGlobalTimeline -join ",")) {
    throw "Side-Star and Giant-Face global timeline changed."
}

Write-Output "Side-Star access semantics verified."
Write-Output (
    "Fixed slots 10/12 at (38,372)/(238,308) map the lower-left and " +
    "upper-right Screen-2 Side-Star access surfaces."
)
Write-Output (
    "1277/905 live calls remain unhit and follow the shared four-tick, " +
    "four-appearance idle animation."
)
Write-Output (
    "ROM-bounded hits award 80 stored units; the left requests Giant-Face " +
    "transformation, while the right can select bonus stage 6."
)
Write-Output (
    "Normalized samples: 02:4BD5@1 and 02:4C0A@1, frame 871, " +
    "135 cycles, 3 writes each."
)
