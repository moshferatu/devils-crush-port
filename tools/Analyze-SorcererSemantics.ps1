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
    [string] $ActorWritePath
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

# Slot $16 is the hidden group controller. Slots $17-$1E are the eight
# upper-table Sorcerers, called Warlocks in the detailed player guide.
Assert-RomHex $rom 0x00466B @"
00 16 8E 57 00 00 9E 00 0B E0 83 00
00 17 F8 57 18 01 EE 00 0B 40 82 00
00 18 F8 57 08 01 DE 00 0B 40 82 01
00 19 F8 57 18 01 CE 00 0B 40 82 02
00 1A F8 57 08 01 DE 00 0B 60 82 03
00 1B F8 57 08 01 DE 00 0B 60 82 04
00 1C F8 57 08 01 DE 00 0B 60 82 05
00 1D F8 57 08 01 DE 00 0B 60 82 06
00 1E F8 57 08 01 DE 00 0B 60 82 07
"@ "Sorcerer layout records"

# The controller owns group collision selector $2D0B, live count $2D0F,
# deployment count $2D10, and color state $2D40.
Assert-RomHex $rom 0x00578E @"
A9 28 8D 0B 2D 9C 0F 2D A9 08 8D 10 2D 9E 80 39
9C 40 2D 20 18 C0 20 18 C0 9C 0B 2D 20 18 C0 AD
0F 2D D0 37 A9 80 20 21 C0 20 18 C0 AD 00 2D 0D
01 2D D0 0C 20 81 C0 A0 28 29 01 F0 0B C8 80 08
A0 28 AD 01 2D 10 01 C8 8C 0B 2D A9 08 8D 10 2D
20 18 C0 20 18 C0 9C 0B 2D 80 C1 AD 40 2D DD 80
39 F0 06 9D 80 39 20 3F 59 60
"@ "Sorcerer controller"

# Each child retains its route index, enters from off-table position
# (-10,196), receives a movement vector, and increments the live count.
Assert-RomHex $rom 0x0057F8 @"
9D 00 37 A9 10 9D 00 38 A9 8D 9D 00 3D 20 18 C0
A9 F6 9D 80 31 A9 00 9D 00 32 A9 C4 9D 00 33 A9
00 9D 80 33 BD 80 30 09 60 9D 80 30 AD 0B 2D 9D
80 37 38 A9 08 ED 05 2D 29 07 4A A8 B9 3C 59 9D
80 38 A9 0A 9D 80 39 9E 00 3E 9E 80 3E EE 0F 2D
BC 00 37 B9 34 59 64 2E BC 80 38 C0 01 F0 0D 0A
26 2E C0 80 F0 06 46 2E 6A 46 2E 6A 9D 00 3B A5
2E 9D 80 3B 20 18 C0
"@ "Sorcerer deployment"

# The final state orbits around the Big Star. A hit awards 10 stored units
# and command $29. Counter zero removes one live Sorcerer, adds 50 units,
# applies the group/color rule, and either regenerates or releases the actor.
Assert-RomHex $rom 0x00586F @"
BD 00 3B 1D 80 3B F0 0C BD 00 3B D0 03 DE 80 3B
DE 00 3B 60 20 18 C0 BD 80 30 49 20 9D 80 30 A9
06 20 66 69 B0 01 60 BD 80 30 29 9F 9D 80 30 A9
05 BC 80 37 C0 28 F0 02 A9 0C 9D 00 3E 9E 80 3E
A9 02 9D 00 3C BD 00 38 29 7F 9D 00 38 CE 10 2D
20 18 C0 AD 10 2D D0 10 38 A9 08 ED 05 2D 29 07
4A A8 B9 3C 59 9D 80 38 BD 80 37 20 66 69 BD 80
3A 9D 80 39 BD 00 38 10 1B 29 7F 9D 00 38 A9 0A
85 2F 64 30 20 2A C0 A9 29 20 87 C0 DE 00 3C F0
04 20 F6 7B 60 CE 0F 2D A9 32 85 2F 64 30 20 2A
C0 20 63 C0 AD 40 2D 29 01 F0 03 20 69 C0 20 EC
41 BD 80 30 09 60 9D 80 30 20 18 C0 AD 0B 2D F0
03 4C 08 58 60
"@ "Sorcerer arrival, orbit, and hit rule"

Assert-RomHex $rom 0x005934 @"
00 16 2C 42 58 6E 84 9A 80 01 02 A9 1C 85 2F A9
1E 85 30 20 42 C0 AD 40 2D 20 55 59 A9 1D 4C 5D
C0 64 2E 0A 26 2E 0A 26 2E 0A 26 2E 0A 26 2E 0A
26 2E 18 65 2F 85 2F A5 2E 65 30 85 30 60
"@ "Sorcerer route and color tables"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 23859 -or
    $writes.Count -ne 346654 -or
    $actorWrites.Count -ne 3904
) {
    throw (
        "Expected 23,859 Sorcerer calls, 346,654 scoped writes, and " +
        "3,904 selected state writes."
    )
}

$allowedPcs = @(
    "578E", "57A4", "57A7", "57AD", "57F8",
    "5808", "586F", "5886", "58C2"
)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $x = [Convert]::ToInt32($call.input_x, 16)
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin $allowedPcs -or
        $x -lt 0x16 -or
        $x -gt 0x1E -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Sorcerer call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Sorcerer call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "578E" = 1
    "57A4" = 1
    "57A7" = 1
    "57AD" = 2648
    "57F8" = 8
    "5808" = 8
    "586F" = 1240
    "5886" = 1800
    "58C2" = 18152
} "Sorcerer routine"

$controller = @($calls | Where-Object { $_.logical_pc -eq "57AD" })
$velocityDecay = @($calls | Where-Object { $_.logical_pc -eq "586F" })
$arrival = @($calls | Where-Object { $_.logical_pc -eq "5886" })
$orbit = @($calls | Where-Object { $_.logical_pc -eq "58C2" })
Assert-Histogram $controller "return_instruction_pc" @{
    "57F7" = 2648
} "Sorcerer controller return"
Assert-Histogram $velocityDecay "return_instruction_pc" @{
    "5882" = 1232
    "C5E9" = 8
} "Sorcerer velocity-decay return"
Assert-Histogram $arrival "return_instruction_pc" @{
    "5895" = 1792
    "C5E9" = 8
} "Sorcerer arrival return"
Assert-Histogram $orbit "return_instruction_pc" @{
    "5903" = 18152
} "Sorcerer orbit return"

$initializers = @($calls | Where-Object { $_.logical_pc -eq "57F8" })
for ($index = 0; $index -lt 8; ++$index) {
    $slot = 0x17 + $index
    $slotHex = "{0:X2}" -f $slot
    $parameterHex = "{0:X2}" -f $index
    if (
        $initializers[$index].input_x -ne $slotHex -or
        $initializers[$index].input_a -ne $parameterHex -or
        $initializers[$index].frame -ne "871"
    ) {
        throw "Sorcerer slot $slotHex initializer identity changed."
    }

    $decayCalls = @(
        $velocityDecay | Where-Object { $_.input_x -eq $slotHex }
    )
    $arrivalCalls = @(
        $arrival | Where-Object { $_.input_x -eq $slotHex }
    )
    $orbitCalls = @(
        $orbit | Where-Object { $_.input_x -eq $slotHex }
    )
    $expectedDecay = 1 + 44 * $index
    $expectedArrivalFrame = 941 + 44 * $index
    $expectedOrbitFrame = 1167 + 44 * $index
    $expectedOrbitCalls = 2423 - 44 * $index
    if (
        $decayCalls.Count -ne $expectedDecay -or
        $decayCalls[-1].frame -ne [string] $expectedArrivalFrame -or
        $decayCalls[-1].return_instruction_pc -ne "C5E9" -or
        $arrivalCalls.Count -ne 225 -or
        $arrivalCalls[0].frame -ne [string] ($expectedArrivalFrame + 1) -or
        $arrivalCalls[-1].frame -ne [string] ($expectedArrivalFrame + 225) -or
        $arrivalCalls[-1].return_instruction_pc -ne "C5E9" -or
        $orbitCalls.Count -ne $expectedOrbitCalls -or
        $orbitCalls[0].frame -ne [string] $expectedOrbitFrame -or
        $orbitCalls[-1].frame -ne "3600"
    ) {
        throw "Sorcerer slot $slotHex deployment chronology changed."
    }
}

# The controller starts one eight-member group and all children arrive in
# route-index order. No hit occurs, so the live count remains eight.
$ruleState = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("2D0B", "2D0F", "2D10", "2D40") -and
            [int] $_.frame -ge 871
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):$($_.logical_address):" +
                "$($_.value)"
        }
)
$expectedRuleState = @(
    "871:02:5790:2D0B:28",
    "871:02:5793:2D0F:00",
    "871:02:5798:2D10:08",
    "871:02:579E:2D40:00",
    "873:02:5845:2D0F:01",
    "873:02:5845:2D0F:02",
    "873:02:5845:2D0F:03",
    "873:02:5845:2D0F:04",
    "873:02:5845:2D0F:05",
    "873:02:5845:2D0F:06",
    "873:02:5845:2D0F:07",
    "873:02:5845:2D0F:08",
    "941:02:57A7:2D0B:00",
    "1166:02:58BC:2D10:07",
    "1210:02:58BC:2D10:06",
    "1254:02:58BC:2D10:05",
    "1298:02:58BC:2D10:04",
    "1342:02:58BC:2D10:03",
    "1386:02:58BC:2D10:02",
    "1430:02:58BC:2D10:01",
    "1474:02:58BC:2D10:00"
)
if (($ruleState -join ",") -ne ($expectedRuleState -join ",")) {
    throw "Sorcerer group-state chronology changed."
}

$pointerLow = @{
    "16" = @("A4", "A7", "AD")
    "17" = @("08", "6F", "86", "C2")
    "18" = @("08", "6F", "86", "C2")
    "19" = @("08", "6F", "86", "C2")
    "1A" = @("08", "6F", "86", "C2")
    "1B" = @("08", "6F", "86", "C2")
    "1C" = @("08", "6F", "86", "C2")
    "1D" = @("08", "6F", "86", "C2")
    "1E" = @("08", "6F", "86", "C2")
}
foreach ($slot in $pointerLow.Keys) {
    $slotNumber = [Convert]::ToInt32($slot, 16)
    $highAddress = "3F{0:X2}" -f ($slotNumber + 0x80)
    $low = @(
        $actorWrites |
            Where-Object {
                $_.logical_address -eq "3F$slot" -and
                [int] $_.frame -ge 871 -and
                $_.instruction_logical_pc -eq "C5E1"
            }
    )
    $high = @(
        $actorWrites |
            Where-Object {
                $_.logical_address -eq $highAddress -and
                [int] $_.frame -ge 871 -and
                $_.instruction_logical_pc -eq "C5E6"
            }
    )
    $expectedHigh = if ($slot -eq "16") { "57" } else { "58" }
    if (
        ($low.value -join ",") -ne ($pointerLow[$slot] -join ",") -or
        $high.Count -ne $pointerLow[$slot].Count -or
        @($high | Where-Object { $_.value -ne $expectedHigh }).Count -ne 0
    ) {
        throw "Sorcerer slot $slot dispatch timeline changed."
    }
}

# The deterministic route reaches collision processing but never sets the
# Sorcerer hit flag. Keep the hit, score, audio, completion, color, and
# regeneration branches ROM-bounded and explicitly inactive.
$inactiveWrites = @(
    $writes |
        Where-Object {
            (
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -in @("58EF", "5904", "5909")
            ) -or
            $_.instruction_logical_pc -eq "ECFD"
        }
)
if ($inactiveWrites.Count -ne 0) {
    throw "Sorcerer hit/completion/audio branch became active."
}

Assert-Call $calls[0] 1 871 "578E" "16" "C5E9" 279 12 (
    "Sorcerer controller initializer"
)
Assert-Call $controller[0] 28 942 "57AD" "16" "57F7" 69 0 (
    "Sorcerer controller normalized sample"
)
Assert-Call $arrival[0] 29 942 "5886" "17" "5895" 762 15 (
    "Sorcerer arrival normalized sample"
)
Assert-Call $orbit[0] 2054 1167 "58C2" "17" "5903" 306 7 (
    "Sorcerer orbit normalized sample"
)

Write-Output "Sorcerer semantics verified."
Write-Output (
    "Controller slot 16 deploys fixed Sorcerer slots 17-1E in eight " +
    "44-frame-staggered paths."
)
Write-Output (
    "All eight reach orbit dispatch 02:58C2; live count remains 8 and " +
    "deployment count reaches 0."
)
Write-Output (
    "ROM-bounded inactive rule: two 10-unit hits, command 29, 50-unit " +
    "completion, color progression, and group regeneration."
)
Write-Output (
    "Normalized samples: controller 02:57AD@1, arrival 02:5886@1, and " +
    "orbit 02:58C2@1."
)
