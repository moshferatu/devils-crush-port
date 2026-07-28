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

# These records sit at the outer edges of the lower side shoots, matching the
# detailed guide's two Side Spiders rather than the inner Front Spiders.
Assert-RomHex $rom 0x004743 @"
00 28 FA 64 28 00 82 02 29 82 83 00
00 29 FA 64 F0 00 82 02 29 82 83 01
"@ "Side-Spider layout records"

# The common initializer preserves the left/right record parameter, installs
# collision selector $10 and completion selector $8C, and dispatches to
# 03:651F.
Assert-RomHex $rom 0x0064FA @"
9D 80 3C A9 10 9D 00 38 A9 8C 9D 00 3D A9 80 9D 80
38 A9 01 9D 00 3A BD 00 38 29 7F 9D 00 38 9E 80 39
20 18 C0
"@ "Side-Spider initialization"

# Steady dispatch chooses collision ID $2C/$2D from the side parameter. A
# collision sets the shared transition latch, emits command $34, awards 100
# stored score units, and runs the same multi-stage disappearance vocabulary
# used by the neighboring Front Spiders.
Assert-RomHex $rom 0x00651F @"
A9 2C BC 80 3C F0 02 A9 2D 20 66 69 20 A4 65 BD 00
38 30 01 60 29 7F 9D 00 38 A9 01 8D 32 2D 20 18 C0
A9 34 20 87 C0 A9 64 85 2F 64 30 20 2A C0 A9 04 9D
80 39 BD 80 30 09 40 9D 80 30 A9 0C 20 21 C0 20 18
C0 FE 80 39 A9 0C 20 21 C0 20 18 C0 BD 80 30 09 20
9D 80 30 A9 30 20 21 C0 20 18 C0 BD 80 30 29 9F 9D
80 30 80 83
"@ "Side-Spider steady hit and transition rule"

# The shared appearance helper advances five four-tick states and returns
# carry set when the current transition completes.
Assert-RomHex $rom 0x006589 @"
DE 00 3A D0 0F A9 04 9D 00 3A FE 80 39 BD 80 39 C9
05 F0 02 18 60 9E 80 39 38 60
"@ "Side-Spider shared appearance helper"
Assert-RomHex $rom 0x00202A @"
4C 4F F5
"@ "Score entry"
Assert-RomHex $rom 0x002087 @"
4C FD EC
"@ "Audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 2798 -or
    $writes.Count -ne 41649 -or
    $stateWrites.Count -ne 7702
) {
    throw (
        "Expected 2,798 calls, 41,649 scoped writes, and 7,702 selected " +
        "Side-Spider state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedReturn = if ($call.logical_pc -eq "64FA") {
        "C5E9"
    } else {
        "6533"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("64FA", "651F") -or
        $call.input_x -notin @("28", "29") -or
        $call.return_instruction_pc -ne $expectedReturn -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Side-Spider call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Side-Spider call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "64FA" = 2
    "651F" = 2796
} "Side-Spider routine"
Assert-Histogram @(
    $calls | Where-Object logical_pc -eq "64FA"
) "input_x" @{
    "28" = 1
    "29" = 1
} "Side-Spider initializer slot"
Assert-Histogram @(
    $calls | Where-Object logical_pc -eq "651F"
) "input_x" @{
    "28" = 1398
    "29" = 1398
} "Side-Spider steady slot"

$initializers = @($calls | Where-Object logical_pc -eq "64FA")
foreach ($call in $initializers) {
    if (
        [int] $call.frame -ne 872 -or
        [int64] $call.body_master_cycles -ne 342 -or
        [int] $call.write_count -ne 14
    ) {
        throw "Side-Spider initializer changed for slot $($call.input_x)."
    }
}

$steady = @($calls | Where-Object logical_pc -eq "651F")
if (
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3548 -or
    ($steady | Measure-Object body_master_cycles -Minimum).Minimum -ne 336 -or
    ($steady | Measure-Object body_master_cycles -Maximum).Maximum -ne 2370 -or
    ($steady | Measure-Object write_count -Minimum).Minimum -ne 9 -or
    ($steady | Measure-Object write_count -Maximum).Maximum -ne 47
) {
    throw "Side-Spider steady frame, timing, or write bounds changed."
}

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "64FA", "64FF", "6504", "6509", "650E",
                "6516", "6519", "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "02:4125:3928:29",
    "02:412B:30A8:82",
    "02:4131:3028:83",
    "03:64FA:3CA8:00",
    "03:64FF:3828:10",
    "03:6504:3D28:8C",
    "03:6509:38A8:80",
    "03:650E:3A28:01",
    "03:6516:3828:10",
    "03:6519:39A8:00",
    "01:C5E1:3F28:1F",
    "01:C5E6:3FA8:65",
    "02:4125:3929:29",
    "02:412B:30A9:82",
    "02:4131:3029:83",
    "03:64FA:3CA9:01",
    "03:64FF:3829:10",
    "03:6504:3D29:8C",
    "03:6509:38A9:80",
    "03:650E:3A29:01",
    "03:6516:3829:10",
    "03:6519:39A9:00",
    "01:C5E1:3F29:1F",
    "01:C5E6:3FA9:65"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Side-Spider initial identity changed."
}

$transitionWrites = @(
    $stateWrites |
        Where-Object logical_address -eq "2D32" |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):$($_.value)"
        }
)
if (
    ($transitionWrites -join ",") -ne
        "1:01:C100:00,871:01:C657:00"
) {
    throw "The routed Side-Spider transition latch became active."
}

$hitMarkers = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "6548" -and
            $_.work_ram_offset -eq "002F" -and
            $_.value -eq "64"
        }
)
if ($hitMarkers.Count -ne 0) {
    throw "The routed Side-Spider collision path became active."
}

if (
    [int64] $steady[0].body_master_cycles -ne 903 -or
    [int] $steady[0].write_count -ne 19
) {
    throw "Side-Spider normalized no-hit sample changed."
}

Write-Output "Side-Spider semantics verified."
Write-Output (
    "Fixed slots 28/29 at (40/240,642) share initializer 03:64FA and " +
    "steady dispatch 03:651F."
)
Write-Output (
    "2796 steady calls split equally across the two side shoots and " +
    "remain collision-free on the routed session."
)
Write-Output (
    "ROM-bounded use selects collision 2C/2D, command 34, a 100-unit " +
    "award, and shared transition latch 2D32."
)
Write-Output (
    "Normalized sample: 03:651F@1, frame 873, 903 cycles, 19 writes."
)
