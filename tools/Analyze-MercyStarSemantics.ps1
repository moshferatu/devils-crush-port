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

# These three adjacent type-$84 records sit in front of the lower-table
# center hole, matching the detailed guide's "3 Mercy Stars."
Assert-RomHex $rom 0x0049D7 @"
00 5F 9F 4F 84 00 94 02 43 80 84 00
00 60 9F 4F 94 00 94 02 43 80 84 00
00 61 9F 4F A4 00 94 02 43 80 84 00
"@ "Mercy-Star layout records"

# The common initializer rejects an ineligible session through the generic
# clear. Otherwise it configures the star appearance, marks the actor
# dormant, and installs steady dispatch 02:4FC1.
Assert-RomHex $rom 0x004F9F @"
AD 2D 22 F0 03 4C 27 C0 A9 16 9D 80 3C A9 01 9D 00
37 A9 05 9D 80 37 BD 80 30 09 60 9D 80 30 20 18 C0
"@ "Mercy-Star initialization"

# Activation requires the normal table mode, exactly one ball remaining,
# and a selected-player score below stored $001388 (500,000 displayed).
Assert-RomHex $rom 0x004FC1 @"
AD 54 22 D0 1B AD 52 2D 3A D0 15 AC 51 22 38 B9 05
22 E9 88 B9 0C 22 E9 13 B9 13 22 E9 00 90 01 60
"@ "Mercy-Star final-ball and score gate"
Assert-RomHex $rom 0x004FE2 @"
BD 80 30 29 9F 9D 80 30 BD 00 38 29 7F 9D 00 38
A9 01 9D 00 3A A9 C1 9D 00 3E A9 50 9D 80 3E 20
18 C0
"@ "Mercy-Star activation state"

# A collision awards 10 stored units (1,000 displayed points), advances the
# shared presentation, moves control into the table-transition sequence,
# sets the Three-Blobs safety latch, and releases the triggering star.
Assert-RomHex $rom 0x005004 @"
BD 00 38 30 0D AD 4E 2D C9 02 D0 03 4C 27 C0 4C DA
4C A9 0A 85 2F 64 30 20 2A C0 A9 04 9D 00 3A 20
6F 40 A9 04 8D 80 39 20 18 C0 AD 80 39 C9 06 F0
0E DE 00 3A D0 08 A9 04 9D 00 3A EE 80 39 60
"@ "Mercy-Star collision and animation"
Assert-RomHex $rom 0x005044 @"
AD 80 30 09 20 8D 80 30 A9 08 20 21 C0 20 18 C0
AD 00 33 E9 08 8D 00 33 AD 80 33 E9 00 8D 80 33
AD 00 3F 9D 80 3C AD 80 3F 9D 00 3C A9 C9 8D 00
3F A9 50 8D 80 3F 9C 80 3C A9 01 8D 4E 2D 20 18
C0
"@ "Mercy-Star table transition"
Assert-RomHex $rom 0x005085 @"
AD 4E 2D F0 01 60 A9 01 8D FD 2C A9 A6 8D 80 31
9C 00 32 A9 90 8D 00 33 9C 80 33 AD 00 38 29 FE
8D 00 38 A9 06 8D 80 39 A9 01 8D 80 3C BD 80 3C
8D 00 3F BD 00 3C 8D 80 3F 4C 53 4D A9 02 8D 4E
2D
"@ "Mercy-Star safety and ball release"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 7953 -or
    $writes.Count -ne 2206 -or
    $stateWrites.Count -ne 1696
) {
    throw (
        "Expected 7,953 calls, 2,206 scoped writes, and 1,696 selected " +
        "Mercy-Star state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $expectedReturn = if ($call.logical_pc -eq "4F9F") {
        "C5E9"
    } else {
        "4FE1"
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("4F9F", "4FC1") -or
        $call.input_x -notin @("5F", "60", "61") -or
        $call.return_instruction_pc -ne $expectedReturn -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Mercy-Star call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Mercy-Star call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "4F9F" = 3
    "4FC1" = 7950
} "Mercy-Star routine"
Assert-Histogram @(
    $calls | Where-Object logical_pc -eq "4F9F"
) "input_x" @{
    "5F" = 1
    "60" = 1
    "61" = 1
} "Mercy-Star initializer slot"
Assert-Histogram @(
    $calls | Where-Object logical_pc -eq "4FC1"
) "input_x" @{
    "5F" = 2650
    "60" = 2650
    "61" = 2650
} "Mercy-Star steady slot"

$initializers = @($calls | Where-Object logical_pc -eq "4F9F")
foreach ($call in $initializers) {
    if (
        [int] $call.frame -ne 872 -or
        [int64] $call.body_master_cycles -ne 318 -or
        [int] $call.write_count -ne 11
    ) {
        throw "Mercy-Star initializer call changed for slot $($call.input_x)."
    }
}

$steady = @($calls | Where-Object logical_pc -eq "4FC1")
if (
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Mercy-Star steady frame bounds changed."
}
$leafCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 54 -and
            [int] $_.write_count -eq 0
        }
)
if ($leafCalls.Count -ne 7787) {
    throw "Mercy-Star dormant leaf population changed."
}
$interruptCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -ne 54 -or
            [int] $_.write_count -ne 0
        }
)
if (
    $interruptCalls.Count -ne 163 -or
    ($interruptCalls | Measure-Object body_master_cycles -Minimum).Minimum -ne
        576 -or
    ($interruptCalls | Measure-Object body_master_cycles -Maximum).Maximum -ne
        1425 -or
    ($interruptCalls | Measure-Object write_count -Minimum).Minimum -ne 10 -or
    ($interruptCalls | Measure-Object write_count -Maximum).Maximum -ne 23
) {
    throw "Mercy-Star interrupt-call population changed."
}

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "4FA9", "4FBB", "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "02:4125:395F:43",
    "02:412B:30DF:80",
    "02:4131:305F:84",
    "02:4FA9:3CDF:16",
    "02:4FBB:30DF:E0",
    "01:C5E1:3F5F:C1",
    "01:C5E6:3FDF:4F",
    "02:4125:3960:43",
    "02:412B:30E0:80",
    "02:4131:3060:84",
    "02:4FA9:3CE0:16",
    "02:4FBB:30E0:E0",
    "01:C5E1:3F60:C1",
    "01:C5E6:3FE0:4F",
    "02:4125:3961:43",
    "02:412B:30E1:80",
    "02:4131:3061:84",
    "02:4FA9:3CE1:16",
    "02:4FBB:30E1:E0",
    "01:C5E1:3F61:C1",
    "01:C5E6:3FE1:4F"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Mercy-Star initial identity changed."
}

$ballCountWrites = @(
    $stateWrites |
        Where-Object logical_address -eq "2D52" |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):$($_.value)"
        }
)
if (($ballCountWrites -join ",") -ne "1:01:C100:00,873:00:F916:03") {
    throw "Mercy-Star routed ball-count gate changed."
}

$activeStateWrites = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -match "^(4F[E-F]|50)"
        }
)
if ($activeStateWrites.Count -ne 0) {
    throw "The routed Mercy-Star activation/use path became active."
}

if (
    [int64] $steady[0].body_master_cycles -ne 54 -or
    [int] $steady[0].write_count -ne 0
) {
    throw "Mercy-Star normalized dormant sample changed."
}

Write-Output "Mercy-Star semantics verified."
Write-Output (
    "Fixed slots 5F-61 at (132/148/164,660) share initializer 02:4F9F " +
    "and steady dispatch 02:4FC1."
)
Write-Output (
    "7950 steady calls remain dormant: 7787 are 54-cycle zero-write " +
    "leaves and 163 contain periodic interrupts."
)
Write-Output (
    "ROM-bounded activation requires the last ball and a score below " +
    "500000; use awards 1000 points and sets Three-Blobs safety."
)
Write-Output (
    "Normalized sample: 02:4FC1@1, frame 873, 54 cycles, 0 writes."
)
