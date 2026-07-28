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
        [string] $X,
        [string] $Return,
        [int64] $Cycles,
        [int] $Writes,
        [string] $Label
    )

    if (
        [int] $Call.sequence -ne $Sequence -or
        [int] $Call.frame -ne $Frame -or
        $Call.logical_pc -ne $Pc -or
        $Call.input_x -ne $X -or
        $Call.return_instruction_pc -ne $Return -or
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

# These two type-$84 fixed records are the left/right channels at the top of
# the Giant Face helmet.
Assert-RomHex $rom 0x00475B @"
00 2A 5C 4A 86 00 24 01 16 A2 84 00
00 2B 64 4A 9E 00 24 01 16 A2 84 00
"@ "Giant-Face helmet-channel layout records"

# The mirrored initializers assign channel selectors $13/$1D and enter the
# shared setup at 02:4C78.
Assert-RomHex $rom 0x004A5C @"
A9 13 9D 80 3C 4C 78 4C A9 1D 9D 80 3C 4C 78 4C
"@ "Giant-Face left/right helmet initializers"

# Shared setup installs the four-tick idle animation and 02:4C91. A collision
# can set Giant-Face request bit 0 after evaluating the special-condition
# state. The no-hit path cycles appearances 0-3.
Assert-RomHex $rom 0x004C78 @"
A9 01 9D 00 3A A9 78 9D 00 3E A9 4C 9D 80 3E A9
91 9D 00 3F A9 4C 9D 80 3F BD 00 38 30 02 80 42
AC 51 22 B9 05 22 19 0C 22 19 13 22 D0 0A A9 02
85 CA A5 53 09 04 85 53 AD 16 2D 3A D0 14 AD 17
2D 3A D0 0E AD 0D 2D C9 08 90 07 A9 01 85 CC 1A
85 CD 20 69 C0 AD 0C 2D 29 FD 09 01 8D 0C 2D 4C
FB 4C DE 00 3A D0 12 A9 04 9D 00 3A FE 80 39 BD
80 39 C9 03 90 03 9E 80 39 60
"@ "Giant-Face helmet setup, activation, and idle animation"

# The activation sequence animates the channel, marks it open, and for these
# original 4C78 actors emits command $20 plus a 3-unit award. It then emits
# command $25, routes the ball using the channel selector, and restores the
# steady dispatch.
Assert-RomHex $rom 0x004CFB @"
A9 04 9D 00 3A 20 6F 40 A9 04 8D 80 39 20 18 C0
AD 80 39 C9 06 F0 0E DE 00 3A D0 08 A9 04 9D 00
3A EE 80 39 60 AD 80 30 09 20 8D 80 30 A9 60 20
21 C0 BD 00 3E C9 78 D0 1F BD 80 3E C9 4C D0 18
BD 80 3D 29 07 D0 11 A9 20 20 87 C0 20 69 78 A9
03 85 2F 64 30 20 2A C0 20 18 C0 AD 80 30 29 DF
8D 80 30 A9 04 9D 00 3A A9 25 20 87 C0 20 18 C0
AD 80 39 C9 03 F0 0E DE 00 3A D0 08 A9 04 9D 00
3A CE 80 39 60 DA 02 A2 00 A5 83 9D 80 38 B9 80
3C 20 75 C0 20 1E C0 AD 80 3C 29 FE 8D 80 3C FA
BD 80 30 09 40 9D 80 30 A9 08 20 21 C0 20 18 C0
BD 80 30 29 BF 9D 80 30 BD 00 38 29 7F 9D 00 38
BD 00 3E 9D 00 3F BD 80 3E 9D 80 3F 60
"@ "Giant-Face helmet activation and ball-return sequence"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 1752 -or
    $writes.Count -ne 3558 -or
    $stateWrites.Count -ne 3934
) {
    throw (
        "Expected 1,752 calls, 3,558 scoped writes, and 3,934 selected " +
        "helmet-channel state writes."
    )
}

$allowedPcs = @("4A5C", "4A64", "4C78", "4C91")
foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $validSlot = switch ($call.logical_pc) {
        "4A5C" { $call.input_x -eq "2A" }
        "4A64" { $call.input_x -eq "2B" }
        default { $call.input_x -in @("2A", "2B") }
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin $allowedPcs -or
        -not $validSlot -or
        $call.return_instruction_pc -ne "4CF1" -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Giant-Face helmet call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Giant-Face helmet call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "4A5C" = 1
    "4A64" = 1
    "4C78" = 2
    "4C91" = 1748
} "Giant-Face helmet routine"

$steady = @($calls | Where-Object logical_pc -eq "4C91")
Assert-Histogram $steady "input_x" @{
    "2A" = 874
    "2B" = 874
} "Giant-Face helmet steady slot"
if (
    ($steady | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Minimum).Minimum -ne 66 -or
    ($steady | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Maximum).Maximum -ne 1134 -or
    ($steady | ForEach-Object {
        [int] $_.write_count
    } | Measure-Object -Minimum).Minimum -ne 1 -or
    ($steady | ForEach-Object {
        [int] $_.write_count
    } | Measure-Object -Maximum).Maximum -ne 20
) {
    throw "Giant-Face helmet steady timing/write range changed."
}

# Initial identity includes type $84, side selectors $13/$1D, timer 1,
# original-state marker 4C78, and live dispatch 4C91.
$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in @(
                "4131", "4A5E", "4A66", "4C7A",
                "4C7F", "4C84", "4C89", "4C8E"
            )
        } |
        ForEach-Object {
            "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "4131:302A:84",
    "4A5E:3CAA:13",
    "4C7A:3A2A:01",
    "4C7F:3E2A:78",
    "4C84:3EAA:4C",
    "4C89:3F2A:91",
    "4C8E:3FAA:4C",
    "4131:302B:84",
    "4A66:3CAB:1D",
    "4C7A:3A2B:01",
    "4C7F:3E2B:78",
    "4C84:3EAB:4C",
    "4C89:3F2B:91",
    "4C8E:3FAB:4C"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Giant-Face helmet initial identity changed."
}

# Both idle channels run the same four-tick timer and appearance cycle.
Assert-Histogram @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in @("4CDA", "4CE1", "4CE4", "4CEE")
        }
) "instruction_logical_pc" @{
    "4CDA" = 1748
    "4CE1" = 438
    "4CE4" = 438
    "4CEE" = 146
} "Giant-Face helmet idle animation write"

$timerDecrements = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4CDA"
        }
)
Assert-Histogram $timerDecrements "logical_address" @{
    "3A2A" = 874
    "3A2B" = 874
} "Giant-Face helmet timer slot"
Assert-Histogram $timerDecrements "value" @{
    "00" = 438
    "01" = 436
    "02" = 436
    "03" = 438
} "Giant-Face helmet timer value"

$appearanceAdvances = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4CE4"
        }
)
Assert-Histogram $appearanceAdvances "logical_address" @{
    "39AA" = 219
    "39AB" = 219
} "Giant-Face helmet appearance slot"
Assert-Histogram $appearanceAdvances "value" @{
    "01" = 146
    "02" = 146
    "03" = 146
} "Giant-Face helmet appearance advance"
$appearanceResets = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4CEE"
        }
)
if (
    $appearanceResets.Count -ne 146 -or
    @($appearanceResets | Where-Object value -ne "00").Count -ne 0
) {
    throw "Giant-Face helmet appearance reset changed."
}

# The route never collides with either channel: no wake request, score, audio,
# open-state transition, or ball routing executes.
$inactivePcs = @(
    "4CA9", "4CAD", "4CC4", "4CCD", "4CD4",
    "4D23", "4D43", "4D4B", "4D5F", "4D80"
)
$inactiveWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in $inactivePcs
        }
)
if ($inactiveWrites.Count -ne 0) {
    throw "Giant-Face helmet activation path became active."
}
if (
    @(
        $stateWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "02" -and
                $_.logical_address -eq "2D0C"
            }
    ).Count -ne 0
) {
    throw "Giant-Face helmet unexpectedly requested a form change."
}

Assert-Call $calls[0] 1 872 "4A5C" "2A" "4CF1" 273 9 (
    "Left Giant-Face helmet initializer"
)
Assert-Call $calls[3] 4 872 "4A64" "2B" "4CF1" 273 9 (
    "Right Giant-Face helmet initializer"
)
Assert-Call $steady[0] 3 872 "4C91" "2A" "4CF1" 135 3 (
    "Giant-Face helmet normalized idle sample"
)

Write-Output "Giant-Face helmet-channel semantics verified."
Write-Output (
    "Fixed slots 2A/2B are mirrored type-84 helmet channels with selectors " +
    "13/1D and shared dispatch 02:4C91."
)
Write-Output (
    "1748 idle calls run identical four-tick, four-appearance cycles; " +
    "the route records no channel collision."
)
Write-Output (
    "ROM-bounded activation sets Giant-Face request bit 0, awards 3 with " +
    "command 20, emits command 25, and returns the ball through its side."
)
Write-Output (
    "Normalized sample: 02:4C91@1, frame 872, 135 cycles, 3 writes."
)
