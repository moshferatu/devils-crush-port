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

# Fixed slots $34/$35 are the lower-left and upper-right Side Stars. Their
# positions and source identity match the two five-member Small-Soldier groups.
Assert-RomHex $rom 0x0047D3 @"
02 34 4D 7D 20 00 5E 01 1A E8 82 00
02 35 E9 7D E8 00 1E 01 1A E8 82 00
"@ "Small-Soldier Side-Star layout records"

# The left source selects collision IDs $0A/$0B, group counter $2D16, subtype
# $88, and five pending children. It spawns every 48 timer ticks, waits while
# the live count is nonzero, then awards 200 units and cycles appearance before
# restarting a cleared group.
Assert-RomHex $rom 0x007D4D @"
A9 0A 9D 00 37 A9 0B 9D 80 37 9E 80 34 9C 16 2D
A9 01 9D 00 3A A9 88 9D 00 3D A9 05 9D 00 3B 20
18 C0 DE 00 3A D0 0D A9 30 9D 00 3A 20 87 7E DE
00 3B F0 01 60 20 18 C0 AD 16 2D F0 01 60 A9 C8
85 2F 64 30 20 2A C0 BD 80 39 C9 01 D0 03 20 66
C0 FE 80 39 BD 80 39 C9 04 D0 03 9E 80 39 20 D2
7F BD 80 39 0A A8 B9 E1 7D 9D 00 37 B9 E2 7D 9D
80 37 A9 C0 9D 00 3B A9 00 9D 80 3B 20 18 C0 BD
00 3B D0 03 DE 80 3B 3A 9D 00 3B 1D 80 3B D0 03
4C 5A 7D 60 0A 0B 07 08 0A 0B 07 08
"@ "Left Small-Soldier Side-Star source"

# The mirrored source selects collision IDs $0C/$0D, group counter $2D17,
# side index 1, and subtype $89. Its lifecycle is otherwise the same.
Assert-RomHex $rom 0x007DE9 @"
A9 0C 9D 00 37 A9 0D 9D 80 37 A9 01 9D 80 34 9C
17 2D A9 89 9D 00 3D A9 01 9D 00 3A A9 05 9D 00
3B 20 18 C0 DE 00 3A D0 0D A9 30 9D 00 3A 20 87
7E DE 00 3B F0 01 60 20 18 C0 AD 17 2D F0 01 60
A9 C8 85 2F 64 30 20 2A C0 BD 80 39 C9 01 D0 03
20 66 C0 FE 80 39 BD 80 39 C9 04 D0 03 9E 80 39
20 D2 7F BD 80 39 0A A8 B9 7F 7E 9D 00 37 B9 80
7E 9D 80 37 A9 C0 9D 00 3B A9 00 9D 80 3B 20 18
C0 BD 00 3B D0 03 DE 80 3B 3A 9D 00 3B 1D 80 3B
D0 03 4C F8 7D 60 09 23 0C 0D 09 23 0C 0D
"@ "Right Small-Soldier Side-Star source"

# The shared spawner allocates type $82, copies position/collision/subtype and
# side identity from the source, sets the two-hit counter to 2, installs
# 03:7EFA, refreshes the child, and increments $2D16,Y.
Assert-RomHex $rom 0x007E87 @"
20 24 C0 90 01 60 A9 82 99 00 30 BD 80 31 99 80
31 BD 00 32 99 00 32 BD 00 33 99 00 33 BD 80 33
99 80 33 BD 00 39 99 00 39 A9 10 99 00 38 BD 00
3D 99 00 3D BD 80 30 29 FE 99 80 30 A9 10 99 80
3D BD 00 37 99 00 37 BD 80 37 99 80 37 BD 80 34
99 80 34 A9 02 99 00 3C A9 FA 99 00 3F A9 7E
99 80 3F DA 02 20 6C C0 FA BC 80 34 B9 16 2D 1A
99 16 2D 60
"@ "Small-Soldier shared spawner"

# Both completion forms reach 03:7F81. Subtype $0F rewrites this pointer and
# holds. Once the animation changes subtype, the routine awards 50 units,
# creates the impact effect, decrements the side-selected group count, and
# clears the actor.
Assert-RomHex $rom 0x007F81 @"
BD 00 3D 29 0F C9 0F D0 0B A9 81 9D 00 3F A9 7F
9D 80 3F 60 A9 32 85 2F 64 30 20 2A C0 20 63 C0
20 EC 41 BC 80 34 B9 16 2D 3A 99 16 2D 4C 27 C0
"@ "Small-Soldier completion and subtype holding state"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 5322 -or
    $writes.Count -ne 3324 -or
    $actorWrites.Count -ne 3888
) {
    throw (
        "Expected 5,322 calls, 3,324 scoped writes, and 3,888 selected " +
        "state writes."
    )
}

$allowedPcs = @("7D4D", "7D6F", "7D85", "7DE9", "7E0D", "7E23", "7E87", "7F81")
foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin $allowedPcs -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Small-Soldier source call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Small-Soldier source call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "7D4D" = 1
    "7D6F" = 193
    "7D85" = 2457
    "7DE9" = 1
    "7E0D" = 193
    "7E23" = 2457
    "7E87" = 10
    "7F81" = 10
} "Small-Soldier source routine"

foreach ($pc in @("7D4D", "7D6F", "7D85")) {
    if (@($calls | Where-Object { $_.logical_pc -eq $pc -and $_.input_x -ne "34" }).Count -ne 0) {
        throw "Left Side-Star source slot identity changed at $pc."
    }
}
if (
    @(
        $calls |
            Where-Object {
                $_.logical_pc -eq "7E87" -and
                $_.input_x -notin @("34", "35")
            }
    ).Count -ne 0
) {
    throw "Small-Soldier spawner source-slot identity changed."
}
foreach ($pc in @("7DE9", "7E0D", "7E23")) {
    if (@($calls | Where-Object { $_.logical_pc -eq $pc -and $_.input_x -ne "35" }).Count -ne 0) {
        throw "Right Side-Star source slot identity changed at $pc."
    }
}
if (
    @(
        $calls |
            Where-Object {
                $_.logical_pc -eq "7F81" -and
                $_.input_x -notin @("62", "69")
            }
    ).Count -ne 0
) {
    throw "Small-Soldier completion slot identity changed."
}

Assert-Histogram @($calls | Where-Object logical_pc -eq "7D6F") "return_instruction_pc" @{
    "7D81" = 192
    "C5E9" = 1
} "Left Side-Star delayed-state return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "7E0D") "return_instruction_pc" @{
    "7E1F" = 192
    "C5E9" = 1
} "Right Side-Star delayed-state return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "7D85") "return_instruction_pc" @{
    "7D8A" = 2457
} "Left Side-Star steady-state return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "7E23") "return_instruction_pc" @{
    "7E28" = 2457
} "Right Side-Star steady-state return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "7F81") "return_instruction_pc" @{
    "7F94" = 8
    "C594" = 2
} "Small-Soldier completion return"

# Initializers establish mirrored collision IDs, group selectors, five pending
# children, and source subtypes $88/$89.
$initializerState = @(
    $writes |
        Where-Object {
            $_.call_sequence -in @("1", "2") -and
            $_.instruction_physical_bank -eq "03" -and
            [Convert]::ToInt32($_.work_ram_offset, 16) -ge 0x0D00
        } |
        ForEach-Object {
            "$($_.call_sequence):$($_.instruction_logical_pc):" +
                "$($_.work_ram_offset):$($_.value)"
        }
)
$expectedInitializerState = @(
    "1:7D4F:1734:0A",
    "1:7D54:17B4:0B",
    "1:7D57:14B4:00",
    "1:7D5A:0D16:00",
    "1:7D5F:1A34:01",
    "1:7D64:1D34:88",
    "1:7D69:1B34:05",
    "2:7DEB:1735:0C",
    "2:7DF0:17B5:0D",
    "2:7DF5:14B5:01",
    "2:7DF8:0D17:00",
    "2:7DFD:1D35:89",
    "2:7E02:1A35:01",
    "2:7E07:1B35:05"
)
if (($initializerState -join ",") -ne ($expectedInitializerState -join ",")) {
    throw "Small-Soldier Side-Star initializer state changed."
}

# The timer creates one child per source at each of five frames. Allocation
# order pairs left/right sources with ten deterministic dynamic slots.
$spawners = @($calls | Where-Object logical_pc -eq "7E87")
$spawnerContexts = @(
    $spawners |
        ForEach-Object {
            "$($_.sequence):$($_.frame):$($_.input_x):" +
                "$($_.body_master_cycles):$($_.write_count)"
        }
)
$expectedSpawnerContexts = @(
    "3:873:34:5136:26",
    "5:873:35:5181:26",
    "101:988:34:6621:51",
    "103:988:35:7359:64",
    "199:1036:34:8778:89",
    "201:1036:35:10461:119",
    "297:1084:34:7437:57",
    "299:1084:35:8496:75",
    "395:1132:34:7683:63",
    "397:1132:35:7071:51"
)
if (($spawnerContexts -join ",") -ne ($expectedSpawnerContexts -join ",")) {
    throw "Small-Soldier pair-spawn chronology changed."
}

$spawnTypeWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7E8F"
        }
)
if (
    ($spawnTypeWrites.frame -join ",") -ne
        "873,873,988,988,1036,1036,1084,1084,1132,1132" -or
    ($spawnTypeWrites.logical_address -join ",") -ne
        "3062,3063,3065,3066,3068,3069,306B,306C,306E,306F" -or
    ($spawnTypeWrites.value -join ",") -ne
        "82,82,82,82,82,82,82,82,82,82"
) {
    throw "Small-Soldier child allocation identity changed."
}

$spawnLowWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7EE1"
        }
)
$spawnHighWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7EE6"
        }
)
if (
    ($spawnLowWrites.logical_address -join ",") -ne
        "3F62,3F63,3F65,3F66,3F68,3F69,3F6B,3F6C,3F6E,3F6F" -or
    ($spawnLowWrites.value -join ",") -ne
        "FA,FA,FA,FA,FA,FA,FA,FA,FA,FA" -or
    ($spawnHighWrites.logical_address -join ",") -ne
        "3FE2,3FE3,3FE5,3FE6,3FE8,3FE9,3FEB,3FEC,3FEE,3FEF" -or
    ($spawnHighWrites.value -join ",") -ne
        "7E,7E,7E,7E,7E,7E,7E,7E,7E,7E"
) {
    throw "Small-Soldier child initial dispatch changed."
}

# Both live counters rise to five, then the two observed completions reduce
# their respective side counts to four.
$counterTimeline = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("2D16", "2D17") -and
            [int] $_.frame -ge 871
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):$($_.logical_address):" +
                "$($_.value)"
        }
)
$expectedCounterTimeline = @(
    "872:03:7D5A:2D16:00",
    "872:03:7DF8:2D17:00",
    "873:03:7EF6:2D16:01",
    "873:03:7EF6:2D17:01",
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
if (($counterTimeline -join ",") -ne ($expectedCounterTimeline -join ",")) {
    throw "Small-Soldier group-count chronology changed."
}

$sourcePointers = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("3F34", "3FB4", "3F35", "3FB5") -and
            [int] $_.frame -ge 871 -and
            $_.instruction_logical_pc -in @("C5E1", "C5E6")
        } |
        ForEach-Object {
            "$($_.frame):$($_.logical_address):$($_.value)"
        }
)
$expectedSourcePointers = @(
    "872:3F34:6F",
    "872:3FB4:7D",
    "872:3F35:0D",
    "872:3FB5:7E",
    "1132:3F34:85",
    "1132:3FB4:7D",
    "1132:3F35:23",
    "1132:3FB5:7E"
)
if (($sourcePointers -join ",") -ne ($expectedSourcePointers -join ",")) {
    throw "Small-Soldier Side-Star dispatch timeline changed."
}

# Slot $62 takes the immediate completion path. Slot $69 holds for eight
# updates, including a same-frame second entry, then follows the same exit.
$completionCalls = @($calls | Where-Object logical_pc -eq "7F81")
$completionContexts = @(
    $completionCalls |
        ForEach-Object {
            "$($_.sequence):$($_.frame):$($_.input_x):" +
                "$($_.return_instruction_pc):$($_.body_master_cycles):" +
                "$($_.write_count)"
        }
)
$expectedCompletionContexts = @(
    "1837:1852:62:C594:14283:266",
    "2044:1959:69:7F94:75:2",
    "2047:1960:69:7F94:75:2",
    "2050:1961:69:7F94:75:2",
    "2053:1962:69:7F94:75:2",
    "2056:1963:69:7F94:732:14",
    "2059:1964:69:7F94:75:2",
    "2062:1965:69:7F94:75:2",
    "2065:1969:69:7F94:75:2",
    "2068:1969:69:C594:41373:739"
)
if (
    ($completionContexts -join ",") -ne
        ($expectedCompletionContexts -join ",")
) {
    throw "Small-Soldier completion/holding chronology changed."
}

$holdingLow = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7F8C"
        }
)
$holdingHigh = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7F91"
        }
)
if (
    ($holdingLow.call_sequence -join ",") -ne
        "2044,2047,2050,2053,2056,2059,2062,2065" -or
    @($holdingLow | Where-Object {
        $_.work_ram_offset -ne "1F69" -or $_.value -ne "81"
    }).Count -ne 0 -or
    ($holdingHigh.call_sequence -join ",") -ne
        "2044,2047,2050,2053,2056,2059,2062,2065" -or
    @($holdingHigh | Where-Object {
        $_.work_ram_offset -ne "1FE9" -or $_.value -ne "7F"
    }).Count -ne 0
) {
    throw "Small-Soldier subtype-0F holding state changed."
}

$awardFifty = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7F97"
        }
)
$groupDecrements = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7FAB"
        }
)
$scoreWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -in @("F587", "F58F", "F597", "F5A5")
        }
)
$clearWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C581" -and
            [Convert]::ToInt32($_.work_ram_offset, 16) -ge 0x1000
        }
)
if (
    ($awardFifty.call_sequence -join ",") -ne "1837,2068" -or
    @($awardFifty | Where-Object {
        $_.work_ram_offset -ne "002F" -or $_.value -ne "32"
    }).Count -ne 0 -or
    ($groupDecrements.call_sequence -join ",") -ne "1837,2068" -or
    ($groupDecrements.work_ram_offset -join ",") -ne "0D16,0D17" -or
    ($groupDecrements.value -join ",") -ne "04,04" -or
    $scoreWrites.Count -ne 8 -or
    $clearWrites.Count -ne 64
) {
    throw "Small-Soldier completion award/count/free behavior changed."
}
foreach ($sequence in @("1837", "2068")) {
    if (
        @($scoreWrites | Where-Object call_sequence -eq $sequence).Count -ne 4 -or
        @($clearWrites | Where-Object call_sequence -eq $sequence).Count -ne 32
    ) {
        throw "Small-Soldier completion call $sequence changed nested effects."
    }
}
foreach ($write in $clearWrites) {
    $slot = [Convert]::ToInt32($write.work_ram_offset, 16) -band 0x7F
    $expectedSlot = if ($write.call_sequence -eq "1837") { 0x62 } else { 0x69 }
    if ($slot -ne $expectedSlot -or $write.value -ne "00") {
        throw "Small-Soldier completion stopped clearing the owning slot."
    }
}

# Neither five-member group is cleared by this route, so the 200-unit source
# award and appearance/regeneration path remain inactive and ROM-bounded.
$sourceAwards = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -in @("7D8C", "7E2A")
        }
)
if ($sourceAwards.Count -ne 0) {
    throw "Small-Soldier five-member group-clear path became active."
}

Assert-Call $calls[0] 1 872 "7D4D" "34" "C5E9" 327 14 (
    "Left Side-Star initializer"
)
Assert-Call @($calls | Where-Object logical_pc -eq "7D85")[0] 399 1133 (
    "7D85"
) "34" "7D8A" 21 0 "Left Side-Star normalized steady sample"
Assert-Call @($calls | Where-Object logical_pc -eq "7E23")[0] 400 1133 (
    "7E23"
) "35" "7E28" 21 0 "Right Side-Star normalized steady sample"
Assert-Call $spawners[0] 3 873 "7E87" "34" "7EF9" 5136 26 (
    "Small-Soldier spawner normalized sample"
)
Assert-Call $completionCalls[0] 1837 1852 "7F81" "62" "C594" 14283 266 (
    "Small-Soldier completion normalized sample"
)

Write-Output "Small-Soldier Side-Star source semantics verified."
Write-Output (
    "Fixed slots 34/35 create paired five-member groups in dynamic slots " +
    "62/63 through 6E/6F."
)
Write-Output (
    "Group counters 2D16/2D17 rise from 0 to 5; observed completions " +
    "reduce each to 4."
)
Write-Output (
    "7F81: slot 62 exits immediately; subtype-0F slot 69 holds for eight " +
    "updates, then awards 50 and exits."
)
Write-Output (
    "ROM-bounded inactive rule: clearing all five awards 200, cycles the " +
    "Side Star, and regenerates its group."
)
Write-Output (
    "Normalized samples: 03:7D85@1, 03:7E23@1, 03:7E87@1, and " +
    "03:7F81@1."
)
