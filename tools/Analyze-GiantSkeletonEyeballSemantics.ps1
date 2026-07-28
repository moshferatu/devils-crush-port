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

# This category-1 fixed record is one pixel right and 17 pixels below the
# Giant Skeleton's origin. The manual names it Giant Skeleton's Eyeball.
Assert-RomHex $rom 0x004797 @"
01 2F C8 4D E1 00 17 02 08 8B 82 00
"@ "Giant-Skeleton-Eyeball layout record"

# Initialization clears actor flag bit 6, clears the shared egg-completion
# latch and actor state, then installs steady dispatch 02:4DD9.
Assert-RomHex $rom 0x004DC8 @"
BD 80 30 29 BF 9D 80 30 9C 20 2D 9E 00 3E 20 18 C0
"@ "Giant-Skeleton-Eyeball initialization"

# The steady leaf waits on bit 0 of $2D20. A secondary mode flag can jump
# directly into the final animation sequence; otherwise the actor returns.
Assert-RomHex $rom 0x004DD9 @"
AD 20 2D 29 01 D0 0A A5 53 29 02 F0 03 4C 15 4F 60
"@ "Giant-Skeleton-Eyeball activation gate"

# The Dragon's-Egg controller owns the other side of the contract: live
# count zero increments the completion count, sets $2D20 bit 0, and awards
# 100 stored score units before beginning the egg reset sequence.
Assert-RomHex $rom 0x0042F6 @"
AD F4 2C F0 01 60 EE FB 2C AD 20 2D 09 01 8D 20 2D
A9 64 85 2F 64 30 20 2A C0
"@ "Dragon's-Egg completion signal"

# A signaled eyeball starts a timer/appearance state machine. Its later
# states preserve the Skeleton's shared ball coordinates, manipulate the
# mouth presentation, and eventually return to the initializer.
Assert-RomHex $rom 0x004DEA @"
A9 01 9D 00 3A A9 02 9D 00 3E 20 18 C0 DE 00 3A D0
17 A9 08 9D 00 3A FE 80 39 BD 00 38 09 40 9D 00 38
BD 80 39 C9 02 F0 01 60 A9 04 20 9F C0 BD 00 38 29
7F 9D 00 38 20 18 C0
"@ "Giant-Skeleton-Eyeball opening states"
Assert-RomHex $rom 0x004F15 @"
AD 54 22 F0 03 4C 93 4F A9 01 9D 00 3A 9E 80 3A A9
E0 9D 00 3B 9E 80 3B AD 52 2D D0 08 9E 00 3B A9 03
9D 80 3B 20 18 C0 DE 00 3A D0 22 A9 08 9D 00 3A FE
80 3A BD 80 3A C9 04 90 04 62 9D 80 3A A8 B9 9B 4F
9D 80 39 BD 00 38 09 40 9D 00 38 BD 00 3B D0 03 DE
80 3B 3A 9D 00 3B 1D 80 3B F0 01 60 9E 80 39 BD 00
38 09 40 9D 00 38 A9 04 20 A2 C0 20 18 C0 A5 53 29
02 D0 03 4C C8 4D 60 A9 04 20 A2 C0 4C C8 4D
"@ "Giant-Skeleton-Eyeball final and reset states"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 1989 -or
    $writes.Count -ne 503 -or
    $stateWrites.Count -ne 596
) {
    throw (
        "Expected 1,989 calls, 503 scoped writes, and 596 selected " +
        "Giant-Skeleton-Eyeball state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedReturn = if ($call.logical_pc -eq "4DC8") {
        "C5E9"
    } else {
        "4DE9"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("4DC8", "4DD9") -or
        $call.input_x -ne "2F" -or
        $call.return_instruction_pc -ne $expectedReturn -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw (
            "Giant-Skeleton-Eyeball call identity changed at sequence " +
            "$($index + 1)."
        )
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Giant-Skeleton-Eyeball call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "4DC8" = 1
    "4DD9" = 1988
} "Giant-Skeleton-Eyeball routine"

$initializer = @($calls | Where-Object logical_pc -eq "4DC8")
if (
    [int] $initializer[0].frame -ne 872 -or
    [int64] $initializer[0].body_master_cycles -ne 258 -or
    [int] $initializer[0].write_count -ne 10
) {
    throw "Giant-Skeleton-Eyeball initializer call changed."
}

$steady = @($calls | Where-Object logical_pc -eq "4DD9")
if (
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Giant-Skeleton-Eyeball steady frame bounds changed."
}
$leafCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 57 -and
            [int] $_.write_count -eq 0
        }
)
if ($leafCalls.Count -ne 1951) {
    throw "Giant-Skeleton-Eyeball idle leaf population changed."
}
$interruptCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -ne 57 -or
            [int] $_.write_count -ne 0
        }
)
if (
    $interruptCalls.Count -ne 37 -or
    ($interruptCalls | Measure-Object body_master_cycles -Minimum).Minimum -ne
        714 -or
    ($interruptCalls | Measure-Object body_master_cycles -Maximum).Maximum -ne
        1569 -or
    ($interruptCalls | Measure-Object write_count -Minimum).Minimum -ne 12 -or
    ($interruptCalls | Measure-Object write_count -Maximum).Maximum -ne 27
) {
    throw "Giant-Skeleton-Eyeball interrupt-call population changed."
}

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "4DCD", "4DD0", "4DD3",
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
    "02:4125:392F:08",
    "02:412B:30AF:8B",
    "02:4131:302F:82",
    "02:4DCD:30AF:8B",
    "02:4DD0:2D20:00",
    "02:4DD3:3E2F:00",
    "01:C5E1:3F2F:D9",
    "01:C5E6:3FAF:4D"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Giant-Skeleton-Eyeball initial identity changed."
}

$gateWrites = @(
    $stateWrites |
        Where-Object logical_address -eq "2D20" |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):$($_.value)"
        }
)
if (($gateWrites -join ",") -ne "01:C100:00,02:4DD0:00") {
    throw "The routed Dragon's-Egg completion latch became active."
}

$activeStateWrites = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -notin @(
                "4125", "412B", "4131",
                "4DCD", "4DD0", "4DD3"
            )
        }
)
if ($activeStateWrites.Count -ne 0) {
    throw "The routed Giant-Skeleton-Eyeball opening path became active."
}

if (
    [int64] $steady[0].body_master_cycles -ne 57 -or
    [int] $steady[0].write_count -ne 0
) {
    throw "Giant-Skeleton-Eyeball normalized idle sample changed."
}

Write-Output "Giant-Skeleton-Eyeball semantics verified."
Write-Output (
    "Fixed slot 2F at (225,535) maps initializer 02:4DC8 and steady " +
    "dispatch 02:4DD9 to Giant Skeleton's Eyeball."
)
Write-Output (
    "1988 steady calls remain dormant: 1951 are 57-cycle zero-write " +
    "leaves and 37 contain periodic interrupts."
)
Write-Output (
    "Dragon's-Egg count zero sets 2D20 bit 0; the Eyeball consumes that " +
    "signal to begin the Giant Skeleton mouth-opening state machine."
)
Write-Output (
    "Normalized sample: 02:4DD9@1, frame 873, 57 cycles, 0 writes."
)
