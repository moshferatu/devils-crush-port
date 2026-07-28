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

function Assert-InterruptPopulation {
    param(
        [object[]] $Rows,
        [string[]] $NormalPairs,
        [int] $ExpectedCount,
        [int64] $MinimumCycles,
        [int64] $MaximumCycles,
        [int] $MinimumWrites,
        [int] $MaximumWrites,
        [string] $Label
    )

    $interrupts = @(
        $Rows |
            Where-Object {
                "$($_.body_master_cycles):$($_.write_count)" -notin
                    $NormalPairs
            }
    )
    if (
        $interrupts.Count -ne $ExpectedCount -or
        ($interrupts | Measure-Object body_master_cycles -Minimum).Minimum -ne
            $MinimumCycles -or
        ($interrupts | Measure-Object body_master_cycles -Maximum).Maximum -ne
            $MaximumCycles -or
        ($interrupts | Measure-Object write_count -Minimum).Minimum -ne
            $MinimumWrites -or
        ($interrupts | Measure-Object write_count -Maximum).Maximum -ne
            $MaximumWrites
    ) {
        throw "$Label interrupt population changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Four contiguous type-$80 records surround the type-$82 Devil Shooter at
# slot $0E. Their positions and direct dependency on Shooter charge $2D15
# establish them as its presentation/control companions.
Assert-RomHex $rom 0x0045DB @"
00 0A A8 53 F4 00 A4 02 2A 42 80 00
00 0B 94 53 00 01 86 02 06 42 80 00
00 0C FC 53 08 01 AE 02 2B 42 80 00
00 0D 21 54 F8 00 9A 02 2C 42 80 00
"@ "Devil-Shooter companion layout records"

# Slot $0B turns the charge counter into vertical position
# $02A8 + floor($2D15 / 2).
Assert-RomHex $rom 0x005394 @"
20 18 C0 AD 15 2D 4A 18 69 A8 9D 00 33 62 69 02
9D 80 33 60
"@ "Devil-Shooter charge-position companion"

# Slot $0C caches the charge counter and advances a three-frame appearance
# only when that value changes.
Assert-RomHex $rom 0x0053FC @"
AD 15 2D 9D 00 3E 9E 80 39 20 18 C0 AD 15 2D
DD 00 3E F0 10 9D 00 3E FE 80 39 BD 80 39 C9 03
90 03 9E 80 39 60
"@ "Devil-Shooter charge-appearance companion"

# Slots $0A/$0D are mirrored session-gated companions. Both clear visibility,
# initialize an eight-tick animation timer, and wait for $53 bit 3. Their
# active states animate while bits 0/3 remain set, then hide and idle until
# bit 3 requests reinitialization.
Assert-RomHex $rom 0x0053A8 @"
BD 80 30 29 DF 9D 80 30 A9 08 9D 00 3B 9E 80 39
20 18 C0 20 E4 53 A5 53 29 08 D0 01 60 20 18 C0
20 E4 53 A5 53 29 09 F0 01 60 BD 80 30 09 20
9D 80 30 20 18 C0 A5 53 29 08 D0 C5 60
"@ "Left Devil-Shooter session companion"
Assert-RomHex $rom 0x005421 @"
BD 80 30 29 DF 9D 80 30 A9 08 9D 00 3B 9E 80 39
20 18 C0 20 E4 53 A5 53 29 08 D0 01 60 20 18 C0
20 5D 54 A5 53 29 09 F0 01 60 BD 80 30 09 20
9D 80 30 20 18 C0 A5 53 29 08 D0 C5 60
"@ "Right Devil-Shooter session companion"

# The helpers differ only in their number of appearance frames: four at
# 53E4 and five at 545D. These exact scopes contain no score or audio call.
Assert-RomHex $rom 0x0053E4 @"
DE 00 3B D0 12 A9 08 9D 00 3B FE 80 39 BD 80 39
C9 04 D0 03 9E 80 39 60
"@ "Four-frame companion animation"
Assert-RomHex $rom 0x00545D @"
DE 00 3B D0 12 A9 08 9D 00 3B FE 80 39 BD 80 39
C9 05 D0 03 9E 80 39 60
"@ "Five-frame companion animation"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 5231 -or
    $writes.Count -ne 6032 -or
    $stateWrites.Count -ne 8133
) {
    throw (
        "Expected 5,231 calls, 6,032 scoped writes, and 8,133 selected " +
        "Devil-Shooter-companion state writes."
    )
}

$slotByPc = @{
    "53A8" = "0A"
    "53BB" = "0A"
    "53C8" = "0A"
    "53DD" = "0A"
    "5394" = "0B"
    "5397" = "0B"
    "53FC" = "0C"
    "5408" = "0C"
    "5421" = "0D"
    "5434" = "0D"
    "5441" = "0D"
    "5456" = "0D"
}
foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        -not $slotByPc.ContainsKey($call.logical_pc) -or
        $call.input_x -ne $slotByPc[$call.logical_pc] -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw (
            "Devil-Shooter-companion call identity changed at sequence " +
            "$($index + 1)."
        )
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Devil-Shooter companion changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "53A8" = 1
    "53BB" = 3
    "53C8" = 266
    "53DD" = 1055
    "5394" = 1
    "5397" = 1260
    "53FC" = 1
    "5408" = 1288
    "5421" = 1
    "5434" = 3
    "5441" = 267
    "5456" = 1085
} "Devil-Shooter-companion routine"

Assert-Histogram $calls @("logical_pc", "return_instruction_pc") @{
    "53A8, C5E9" = 1
    "53BB, 53C4" = 2
    "53BB, C5E9" = 1
    "53C8, 53D1" = 265
    "53C8, C5E9" = 1
    "53DD, 53E3" = 1055
    "5394, C5E9" = 1
    "5397, 53A7" = 1260
    "53FC, C5E9" = 1
    "5408, 5420" = 1288
    "5421, C5E9" = 1
    "5434, 543D" = 2
    "5434, C5E9" = 1
    "5441, 544A" = 266
    "5441, C5E9" = 1
    "5456, 545C" = 1085
} "Devil-Shooter-companion return"

$initializers = @($calls | Where-Object logical_pc -in @(
    "53A8", "5394", "53FC", "5421"
))
$expectedInitializerCalls = @(
    "53A8:0A:264:10",
    "5394:0B:192:7",
    "53FC:0C:237:9",
    "5421:0D:264:10"
)
$actualInitializerCalls = @(
    $initializers |
        ForEach-Object {
            "$($_.logical_pc):$($_.input_x):" +
                "$($_.body_master_cycles):$($_.write_count)"
        }
)
if (
    @($initializers | Where-Object { [int] $_.frame -ne 871 }).Count -ne 0 -or
    ($actualInitializerCalls -join ",") -ne
        ($expectedInitializerCalls -join ",")
) {
    throw "Devil-Shooter-companion initializer calls changed."
}

Assert-Histogram @($calls | Where-Object logical_pc -eq "53BB") (
    @("body_master_cycles", "write_count")
) @{
    "99, 3" = 2
    "297, 10" = 1
} "Left bootstrap"
Assert-Histogram @($calls | Where-Object logical_pc -eq "5434") (
    @("body_master_cycles", "write_count")
) @{
    "99, 3" = 2
    "297, 10" = 1
} "Right bootstrap"

$leftActive = @($calls | Where-Object logical_pc -eq "53C8")
$rightActive = @($calls | Where-Object logical_pc -eq "5441")
$leftDormant = @($calls | Where-Object logical_pc -eq "53DD")
$rightDormant = @($calls | Where-Object logical_pc -eq "5456")
$position = @($calls | Where-Object logical_pc -eq "5397")
$appearance = @($calls | Where-Object logical_pc -eq "5408")

Assert-Histogram @(
    $leftActive |
        Where-Object {
            "$($_.body_master_cycles):$($_.write_count)" -in @(
                "99:3", "168:5", "177:6", "333:11"
            )
        }
) @("body_master_cycles", "write_count") @{
    "99, 3" = 218
    "168, 5" = 23
    "177, 6" = 8
    "333, 11" = 1
} "Left active path"
Assert-Histogram @(
    $rightActive |
        Where-Object {
            "$($_.body_master_cycles):$($_.write_count)" -in @(
                "99:3", "168:5", "177:6", "333:11"
            )
        }
) @("body_master_cycles", "write_count") @{
    "99, 3" = 226
    "168, 5" = 26
    "177, 6" = 6
    "333, 11" = 1
} "Right active path"

Assert-InterruptPopulation $leftActive @(
    "99:3", "168:5", "177:6", "333:11"
) 16 756 1188 15 23 "Left active"
Assert-InterruptPopulation $rightActive @(
    "99:3", "168:5", "177:6", "333:11"
) 8 756 1557 15 29 "Right active"
Assert-InterruptPopulation $leftDormant @("24:0") 11 681 1203 12 22 (
    "Left dormant"
)
Assert-InterruptPopulation $rightDormant @("24:0") 9 546 909 10 14 (
    "Right dormant"
)
Assert-InterruptPopulation $position @("75:2") 47 597 1407 12 24 (
    "Charge position"
)
Assert-InterruptPopulation $appearance @("42:0", "105:2", "114:3") (
    22
) 699 1494 12 26 "Charge appearance"

if (
    @(
        $leftDormant |
            Where-Object {
                [int64] $_.body_master_cycles -eq 24 -and
                [int] $_.write_count -eq 0
            }
    ).Count -ne 1044 -or
    @(
        $rightDormant |
            Where-Object {
                [int64] $_.body_master_cycles -eq 24 -and
                [int] $_.write_count -eq 0
            }
    ).Count -ne 1076 -or
    @(
        $position |
            Where-Object {
                [int64] $_.body_master_cycles -eq 75 -and
                [int] $_.write_count -eq 2
            }
    ).Count -ne 1213
) {
    throw "Devil-Shooter-companion leaf populations changed."
}
Assert-Histogram @(
    $appearance |
        Where-Object {
            "$($_.body_master_cycles):$($_.write_count)" -in @(
                "42:0", "105:2", "114:3"
            )
        }
) @("body_master_cycles", "write_count") @{
    "42, 0" = 1209
    "105, 2" = 38
    "114, 3" = 19
} "Charge appearance path"

$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 871 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "53AD", "53B2", "53B5",
                "53FF", "5402",
                "5426", "542B", "542E",
                "C5E1", "C5E6"
            ) -and $_.logical_address -in @(
                "300A", "308A", "390A", "398A", "3B0A", "3F0A", "3F8A",
                "300B", "308B", "390B", "3F0B", "3F8B",
                "300C", "308C", "390C", "398C", "3E0C", "3F0C", "3F8C",
                "300D", "308D", "390D", "398D", "3B0D", "3F0D", "3F8D"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "02:4125:390A:2A",
    "02:412B:308A:42",
    "02:4131:300A:80",
    "02:53AD:308A:42",
    "02:53B2:3B0A:08",
    "02:53B5:398A:00",
    "01:C5E1:3F0A:BB",
    "01:C5E6:3F8A:53",
    "02:4125:390B:06",
    "02:412B:308B:42",
    "02:4131:300B:80",
    "01:C5E1:3F0B:97",
    "01:C5E6:3F8B:53",
    "02:4125:390C:2B",
    "02:412B:308C:42",
    "02:4131:300C:80",
    "02:53FF:3E0C:00",
    "02:5402:398C:00",
    "01:C5E1:3F0C:08",
    "01:C5E6:3F8C:54",
    "02:4125:390D:2C",
    "02:412B:308D:42",
    "02:4131:300D:80",
    "02:5426:308D:42",
    "02:542B:3B0D:08",
    "02:542E:398D:00",
    "01:C5E1:3F0D:34",
    "01:C5E6:3F8D:54"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Devil-Shooter-companion initial identity changed."
}

$dispatchTimeline = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -ge 871 -and
            $_.logical_address -in @("3F0A", "3F8A", "3F0D", "3F8D")
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedDispatchTimeline = @(
    "871:C581:3F0A:00",
    "871:C581:3F8A:00",
    "871:C5E1:3F0A:BB",
    "871:C5E6:3F8A:53",
    "871:C581:3F0D:00",
    "871:C581:3F8D:00",
    "871:C5E1:3F0D:34",
    "871:C5E6:3F8D:54",
    "942:C5E1:3F0A:C8",
    "942:C5E6:3F8A:53",
    "942:C5E1:3F0D:41",
    "942:C5E6:3F8D:54",
    "2131:C5E1:3F0D:56",
    "2131:C5E6:3F8D:54",
    "2135:C5E1:3F0A:DD",
    "2135:C5E6:3F8A:53"
)
if (($dispatchTimeline -join ",") -ne ($expectedDispatchTimeline -join ",")) {
    throw "Devil-Shooter-companion dispatch timeline changed."
}

$charge = 0
$lowPositionWrites = 0
$highPositionWrites = 0
$chargeCacheWrites = 0
foreach ($write in $stateWrites) {
    if ($write.logical_address -eq "2D15") {
        $charge = [Convert]::ToInt32($write.value, 16)
    } elseif ($write.instruction_logical_pc -eq "539E") {
        $expected = 0xA8 + [Math]::Floor($charge / 2)
        if ([Convert]::ToInt32($write.value, 16) -ne $expected) {
            throw "Devil-Shooter charge-position low byte changed."
        }
        ++$lowPositionWrites
    } elseif ($write.instruction_logical_pc -eq "53A4") {
        if ($write.value -ne "02") {
            throw "Devil-Shooter charge-position high byte changed."
        }
        ++$highPositionWrites
    } elseif ($write.instruction_logical_pc -eq "5410") {
        if ([Convert]::ToInt32($write.value, 16) -ne $charge) {
            throw "Devil-Shooter charge-appearance cache changed."
        }
        ++$chargeCacheWrites
    }
}
if (
    $lowPositionWrites -ne 1260 -or
    $highPositionWrites -ne 1260 -or
    $chargeCacheWrites -ne 58
) {
    throw "Devil-Shooter charge-projection write counts changed."
}

Assert-Histogram @(
    $stateWrites |
        Where-Object {
            $_.instruction_logical_pc -in @("5413", "541D")
        }
) "instruction_logical_pc" @{
    "5413" = 58
    "541D" = 19
} "Charge-appearance advance"

$normalized = @(
    "53BB:872:99:3",
    "53C8:943:99:3",
    "53DD:2136:24:0",
    "5397:872:75:2",
    "5408:872:105:2",
    "5434:872:99:3",
    "5441:943:99:3",
    "5456:2132:24:0"
)
foreach ($expected in $normalized) {
    $parts = $expected.Split(":")
    $sample = @($calls | Where-Object logical_pc -eq $parts[0])[0]
    if (
        [int] $sample.frame -ne [int] $parts[1] -or
        [int64] $sample.body_master_cycles -ne [int64] $parts[2] -or
        [int] $sample.write_count -ne [int] $parts[3]
    ) {
        throw "Normalized Devil-Shooter companion $($parts[0]) changed."
    }
}

Write-Output "Devil-Shooter companion semantics verified."
Write-Output (
    "Fixed type-80 slots 0A-0D surround Shooter slot 0E and own its " +
    "session animations, charge position, and charge appearance."
)
Write-Output (
    "Slot 0B projects Y=02A8+floor(2D15/2); slot 0C advances modulo " +
    "three on 58 routed charge changes."
)
Write-Output (
    "Slots 0A/0D follow four-/five-frame active animations, then " +
    "transition to zero-write dormant states."
)
Write-Output (
    "Eight normalized samples cover every routed companion state."
)
