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
        [object[]] $Property,
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

# This type-$84 surface is at (38,308), immediately inside the Walled Star at
# (32,302). Its state handshake below establishes the access-pocket identity.
Assert-RomHex $rom 0x00462F @"
00 11 44 4A 26 00 34 01 16 82 84 00
"@ "Walled-Star access layout record"

# Initialization selects appearance $04 and installs the live 02:4B3F
# dispatcher before entering the shared four-tick/four-appearance idle loop.
Assert-RomHex $rom 0x004A44 @"
A9 04 9D 80 3C 4C 26 4B
"@ "Walled-Star access initializer"
Assert-RomHex $rom 0x004B26 @"
A9 01 9D 00 3A A9 26 9D 00 3E A9 4B 9D 80 3E
A9 3F 9D 00 3F A9 4B 9D 80 3F
"@ "Walled-Star access dispatch setup"
Assert-RomHex $rom 0x004CDA @"
DE 00 3A D0 12 A9 04 9D 00 3A FE 80 39 BD 80 39
C9 03 90 03 9E 80 39 60
"@ "Shared access idle animation"

# A collision raises the shared $2DBC latch, awards $64 binary units (100
# stored units, or 10,000 displayed points), then animates for six steps.
Assert-RomHex $rom 0x004B3F @"
BD 00 38 30 03 4C DA 4C A9 01 8D BC 2D A9 64 85 2F
64 30 20 2A C0 A9 04 9D 00 3A 20 6F 40 A9 04 8D 80
39 20 18 C0 AD 80 39 C9 06 F0 0E DE 00 3A D0 08 A9
04 9D 00 3A EE 80 39 60
"@ "Walled-Star access hit and opening animation"

# Completion raises $2D53 bit 0. The Walled-Star post-destruction state waits
# on that same field, then clears $2DBC as its acknowledgment.
Assert-RomHex $rom 0x004B7A @"
AD 80 30 09 20 8D 80 30 AD 53 2D 09 01 8D 53 2D
20 18 C0 AD 53 2D F0 01 60
"@ "Walled-Star access request"
Assert-RomHex $rom 0x00638F @"
AD 53 2D D0 06 AD 32 2D D0 01 60 20 18 C0 AD 53 2D
F0 01 60 9C BC 2D
"@ "Walled-Star access acknowledgment"

# Once acknowledged, the access pocket asks E8A4 for a bounded random value
# with A=6, increments its 0..5 result, and stores bonus-stage ID 1..6 in $55.
Assert-RomHex $rom 0x004B93 @"
AD 80 30 09 20 8D 80 30 A9 06 20 7E C0 1A 85 55
A5 53 09 10 85 53 20 18 C0 A9 06 8D 80 39
AD 80 3C 09 01 8D 80 3C 4C 20 4D
"@ "Walled-Star random bonus-stage selection"
Assert-RomHex $rom 0x0008A4 @"
48 20 B3 E8 85 30 68 85 2F 20 69 E5 A5 30 60
"@ "Bounded RNG wrapper"
Assert-RomHex $rom 0x000569 @"
DA 62 A2 08 0A 26 30 90 07 18 65 2F 90 02 E6 30
CA D0 F1 85 2F FA 60
"@ "Unsigned RNG range scaler"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 906 -or
    $writes.Count -ne 1800 -or
    $stateWrites.Count -ne 2790
) {
    throw (
        "Expected 906 calls, 1,800 scoped writes, and 2,790 selected " +
        "Walled-Star-access state writes."
    )
}

foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("4A44", "4B3F") -or
        $call.input_x -ne "11" -or
        $call.return_instruction_pc -ne "4CF1" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw (
            "Walled-Star-access call identity changed at sequence " +
            "$($index + 1)."
        )
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Walled-Star-access call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "4A44" = 1
    "4B3F" = 905
} "Walled-Star-access routine"

$initializer = @($calls | Where-Object logical_pc -eq "4A44")
if (
    [int] $initializer[0].frame -ne 871 -or
    [int64] $initializer[0].body_master_cycles -ne 273 -or
    [int] $initializer[0].write_count -ne 9
) {
    throw "Walled-Star-access initializer changed."
}

$steady = @($calls | Where-Object logical_pc -eq "4B3F")
if (
    [int] $steady[0].frame -ne 871 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Walled-Star-access steady frame bounds changed."
}

$short = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 66 -and
            [int] $_.write_count -eq 1
        }
)
$advance = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 135 -and
            [int] $_.write_count -eq 3
        }
)
$wrap = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 144 -and
            [int] $_.write_count -eq 4
        }
)
$interrupts = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -notin @(66, 135, 144)
        }
)
if (
    $short.Count -ne 661 -or
    $advance.Count -ne 144 -or
    $wrap.Count -ne 72 -or
    $interrupts.Count -ne 28 -or
    ($interrupts | Measure-Object body_master_cycles -Minimum).Minimum -ne
        588 -or
    ($interrupts | Measure-Object body_master_cycles -Maximum).Maximum -ne
        1023 -or
    ($interrupts | Measure-Object write_count -Minimum).Minimum -ne 11 -or
    ($interrupts | Measure-Object write_count -Maximum).Maximum -ne 17
) {
    throw "Walled-Star-access idle-call population changed."
}

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -in @(871, 872) -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "4A46", "4B28", "4B2D", "4B32", "4B37", "4B3C",
                "6319", "6321", "6326", "632B", "6333",
                "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "871:02:4125:3911:16",
    "871:02:412B:3091:82",
    "871:02:4131:3011:84",
    "871:02:4A46:3C91:04",
    "871:02:4B28:3A11:01",
    "871:02:4B2D:3E11:26",
    "871:02:4B32:3E91:4B",
    "871:02:4B37:3F11:3F",
    "871:02:4B3C:3F91:4B",
    "872:02:4125:395B:03",
    "872:02:412B:30DB:89",
    "872:02:4131:305B:82",
    "872:03:6319:39DB:00",
    "872:03:6321:30DB:89",
    "872:03:6326:385B:10",
    "872:03:632B:3D5B:84",
    "872:03:6333:385B:50",
    "872:01:C5E1:3F5B:39",
    "872:01:C5E6:3FDB:63"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Walled-Star and access initial identity changed."
}

$globalTimeline = @(
    $stateWrites |
        Where-Object {
            $_.logical_address -in @("2D32", "2D53", "2DBC")
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedGlobalTimeline = @(
    "1:01:C100:2D32:00",
    "1:01:C100:2D53:00",
    "1:01:C100:2DBC:00",
    "7:08:560F:2D53:00",
    "180:08:5697:2D53:01",
    "858:08:5854:2D53:00",
    "871:01:C657:2D32:00"
)
if (($globalTimeline -join ",") -ne ($expectedGlobalTimeline -join ",")) {
    throw "Walled-Star access-handshake timeline changed."
}

Assert-Histogram @(
    $stateWrites |
        Where-Object {
            $_.logical_address -in @("3091", "30DB") -and
            $_.instruction_logical_pc -in @("F520", "F54B")
        }
) @("logical_address", "instruction_logical_pc") @{
    "3091, F520" = 309
    "3091, F54B" = 210
    "30DB, F520" = 320
    "30DB, F54B" = 199
} "Walled-Star/access camera-window write"

$hitWrites = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -in @("02", "03") -and
            $_.instruction_logical_pc -in @(
                "4B49", "4B87", "4BA1", "63A3"
            )
        }
)
if ($hitWrites.Count -ne 0) {
    throw "The routed Walled-Star access handshake became active."
}

if (
    [int64] $steady[0].body_master_cycles -ne 135 -or
    [int] $steady[0].write_count -ne 3
) {
    throw "Walled-Star-access normalized idle sample changed."
}

Write-Output "Walled-Star access semantics verified."
Write-Output (
    "Fixed slot 11 at (38,308), immediately inside Walled-Star slot 5B, " +
    "maps initializer 02:4A44 and steady dispatch 02:4B3F."
)
Write-Output (
    "905 live calls remain unhit and follow the shared four-tick, " +
    "four-appearance idle animation."
)
Write-Output (
    "ROM-bounded access awards 100 stored units, handshakes through " +
    "2DBC/2D53, and selects a random bonus stage from 1 through 6."
)
Write-Output (
    "Normalized sample: 02:4B3F@1, frame 871, 135 cycles, 3 writes."
)
