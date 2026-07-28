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

# The category-1 record at world position (64,254) is the upper-left
# screen-2 Giant Door named by the USA manual. It owns fixed slot $4C.
Assert-RomHex $rom 0x0048F3 @"
01 4C 94 62 40 00 FE 00 1E 02 82 00
"@ "Giant-Door layout record"

# Initialization clears four downstream handshake bytes, installs a
# three-hit counter, marks the collision surface, and uses the return address
# after C018 as steady dispatch 03:62B8.
Assert-RomHex $rom 0x006294 @"
9C 0A 2D 9C 06 2D 9C 07 2D 9C 08 2D 9C 09 2D A9 03
9D 00 3B BD 00 38 09 10 9D 00 38 A9 81 9D 00 3D 20
18 C0
"@ "Giant-Door initialization"

# Each collision clears the pending-hit sign bit and awards 50 stored units
# (5,000 displayed points). The third hit awards 500 stored units (50,000
# displayed points), runs the common impact helper, disables the original
# collision surface, and advances to the four-channel completion handshake.
Assert-RomHex $rom 0x0062B8 @"
BD 00 38 10 16 29 7F 9D 00 38 A9 32 85 2F 64 30 20
2A C0 DE 00 3B F0 04 20 F6 7B 60 A9 F4 85 2F A9 01
85 30 20 2A C0 20 44 41 BD 80 30 09 60 9D 80 30 9E
00 3B 20 18 C0 BC 00 3B B9 06 2D 09 01 99 06 2D 20
18 C0 BC 00 3B B9 06 2D 29 02 F0 0C FE 00 3B BD 00
3B C9 04 F0 03 80 D9 60 20 18 C0
"@ "Giant-Door hit and completion states"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 677 -or
    $writes.Count -ne 103 -or
    $stateWrites.Count -ne 579
) {
    throw (
        "Expected 677 calls, 103 scoped writes, and 579 selected " +
        "Giant-Door state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedReturn = if ($call.logical_pc -eq "6294") {
        "C5E9"
    } else {
        "62D3"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("6294", "62B8") -or
        $call.input_x -ne "4C" -or
        $call.return_instruction_pc -ne $expectedReturn -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Giant-Door call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Giant-Door call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "6294" = 1
    "62B8" = 676
} "Giant-Door routine"

$steady = @($calls | Where-Object logical_pc -eq "62B8")
if (
    [int] $steady[0].frame -ne 1228 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Giant-Door steady frame bounds changed."
}
Assert-Histogram $steady "body_master_cycles" @{
    "27" = 669
    "684" = 4
    "720" = 2
    "774" = 1
} "Giant-Door steady cycle"
Assert-Histogram $steady "write_count" @{
    "0" = 669
    "12" = 4
    "13" = 2
    "14" = 1
} "Giant-Door steady write"

# The first 59 selected writes cover global clearing and the layout call. The
# mechanism-owned frame-872 writes establish type $82, fixed position
# (64,254), state $1E, flags $02, hit counter 3, collision selector $10,
# completion selector $81, and live dispatch 03:62B8.
$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "6294", "6297", "629A", "629D", "62A0",
                "62A5", "62AD", "62B2", "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "02:4125:394C:1E",
    "02:412B:30CC:02",
    "02:4131:304C:82",
    "03:6294:2D0A:00",
    "03:6297:2D06:00",
    "03:629A:2D07:00",
    "03:629D:2D08:00",
    "03:62A0:2D09:00",
    "03:62A5:3B4C:03",
    "03:62AD:384C:10",
    "03:62B2:3D4C:81",
    "01:C5E1:3F4C:B8",
    "01:C5E6:3FCC:62"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Giant-Door initial identity changed."
}

# The camera sweep marks the fixed record outside/inside the active window
# 380/139 times. Every routed steady call takes the no-hit return at 62D3;
# the hit counter and four completion bytes therefore remain at their
# initialized values throughout this route.
Assert-Histogram @(
    $stateWrites |
        Where-Object logical_address -eq "30CC"
) "instruction_logical_pc" @{
    "C100" = 1
    "585A" = 1
    "C581" = 1
    "412B" = 1
    "F520" = 380
    "F54B" = 139
} "Giant-Door camera-window write"

$activeMechanismWrites = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -gt 872 -and
            $_.logical_address -in @(
                "2D06", "2D07", "2D08", "2D09", "2D0A",
                "384C", "3B4C", "3D4C", "3F4C", "3FCC"
            )
        }
)
if ($activeMechanismWrites.Count -ne 0) {
    throw "The routed Giant-Door hit/completion path became active."
}

$first = $calls[0]
if (
    [int] $first.frame -ne 872 -or
    [int64] $first.body_master_cycles -ne 345 -or
    [int] $first.write_count -ne 15
) {
    throw "Giant-Door initializer sample changed."
}
if (
    [int64] $steady[0].body_master_cycles -ne 27 -or
    [int] $steady[0].write_count -ne 0
) {
    throw "Giant-Door normalized idle sample changed."
}

Write-Output "Giant-Door semantics verified."
Write-Output (
    "Fixed slot 4C at (64,254) maps initializer 03:6294 and steady " +
    "dispatch 03:62B8 to the manual's Giant Door."
)
Write-Output (
    "676 steady calls take the no-hit return; 669 are 27-cycle, " +
    "zero-write leaf calls and seven contain periodic interrupts."
)
Write-Output (
    "ROM-bounded hits award 50 stored units; the third adds 500 units " +
    "and starts the four-channel completion handshake."
)
Write-Output (
    "Normalized sample: 03:62B8@1, frame 1228, 27 cycles, 0 writes."
)
