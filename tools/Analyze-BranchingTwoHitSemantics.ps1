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

function Assert-RomBytes {
    param(
        [byte[]] $Rom,
        [int] $Offset,
        [byte[]] $Expected,
        [string] $Label
    )

    if ($Offset -lt 0 -or $Offset + $Expected.Count -gt $Rom.Count) {
        throw "$Label is outside the ROM."
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $Expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}: expected {1:X2}, " +
                "found {2:X2}."
            ) -f (
                $Offset + $index
            ), $Expected[$index], $Rom[$Offset + $index]
        }
    }
}

function Import-TraceCsv {
    param(
        [string] $Path
    )

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

# The spawner creates type-$82 actors in pairs, copies source identity fields,
# initializes a two-hit counter, installs 03:7EFA, refreshes activation, and
# increments the group counter selected by $3480,X through $2D16,Y.
Assert-RomBytes $rom 0x007E87 @(
    0x20, 0x24, 0xC0, 0x90, 0x01, 0x60,
    0xA9, 0x82, 0x99, 0x00, 0x30,
    0xBD, 0x80, 0x31, 0x99, 0x80, 0x31,
    0xBD, 0x00, 0x32, 0x99, 0x00, 0x32,
    0xBD, 0x00, 0x33, 0x99, 0x00, 0x33,
    0xBD, 0x80, 0x33, 0x99, 0x80, 0x33,
    0xBD, 0x00, 0x39, 0x99, 0x00, 0x39,
    0xA9, 0x10, 0x99, 0x00, 0x38,
    0xBD, 0x00, 0x3D, 0x99, 0x00, 0x3D,
    0xBD, 0x80, 0x30, 0x29, 0xFE, 0x99, 0x80, 0x30,
    0xA9, 0x10, 0x99, 0x80, 0x3D,
    0xBD, 0x00, 0x37, 0x99, 0x00, 0x37,
    0xBD, 0x80, 0x37, 0x99, 0x80, 0x37,
    0xBD, 0x80, 0x34, 0x99, 0x80, 0x34,
    0xA9, 0x02, 0x99, 0x00, 0x3C,
    0xA9, 0xFA, 0x99, 0x00, 0x3F,
    0xA9, 0x7E, 0x99, 0x80, 0x3F,
    0xDA, 0x02, 0x20, 0x6C, 0xC0, 0xFA,
    0xBC, 0x80, 0x34, 0xB9, 0x16, 0x2D,
    0x1A, 0x99, 0x16, 0x2D, 0x60
) "7E87 branching two-hit spawner"

# The steady state awards 30 and emits command $21 for every hit. Counter zero
# branches on the low nibble of $3D00,X: subtype $0F enters holding dispatch
# 7F81, while other subtypes receive a 50-unit completion award and are freed.
Assert-RomBytes $rom 0x007F57 @(
    0xBD, 0x80, 0x37, 0x20, 0x66, 0x69, 0x20, 0xB1, 0x7F,
    0xBD, 0x00, 0x38, 0x10, 0x1B,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xA9, 0x1E, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x21, 0x20, 0x87, 0xC0,
    0xDE, 0x00, 0x3C, 0xF0, 0x04, 0x20, 0xF6, 0x7B, 0x60,
    0xBD, 0x00, 0x3D, 0x29, 0x0F, 0xC9, 0x0F, 0xD0, 0x0B,
    0xA9, 0x81, 0x9D, 0x00, 0x3F,
    0xA9, 0x7F, 0x9D, 0x80, 0x3F, 0x60,
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x63, 0xC0, 0x20, 0xEC, 0x41,
    0xBC, 0x80, 0x34, 0xB9, 0x16, 0x2D,
    0x3A, 0x99, 0x16, 0x2D, 0x4C, 0x27
) "7F57 branching two-hit update"
Assert-RomBytes $rom 0x002024 @(
    0x4C, 0xAB, 0xC5,
    0x4C, 0x74, 0xC5,
    0x4C, 0x4F, 0xF5
) "C024 allocation, C027 clear, and C02A score entries"
Assert-RomBytes $rom 0x002087 @(
    0x4C, 0xFD, 0xEC
) "C087 audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 20867 -or
    $writes.Count -ne 362060 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 20,867 calls, 362,060 scoped writes, and 3,872 actor " +
        "identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne "7F57" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "7F57 bounded-call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "7F57 changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "input_x" @{
    "62" = 798
    "63" = 2504
    "65" = 2488
    "66" = 2456
    "68" = 2440
    "69" = 773
    "6B" = 2392
    "6C" = 2360
    "6E" = 2344
    "6F" = 2312
} "7F57 slot"
$returnRows = foreach ($call in $calls) {
    [pscustomobject] @{
        Return = (
            $call.return_physical_bank + ":" +
            $call.return_instruction_pc
        )
    }
}
Assert-Histogram $returnRows "Return" @{
    "03:7F80" = 20865
    "03:7F94" = 1
    "01:C594" = 1
} "7F57 outer return"

$bodyCycles = @($calls | ForEach-Object { [int64] $_.body_master_cycles })
$writeCounts = @($calls | ForEach-Object { [int] $_.write_count })
if (
    [int] $calls[0].frame -ne 1054 -or
    [int] $calls[-1].frame -ne 3600 -or
    ($bodyCycles | Measure-Object -Minimum).Minimum -ne 417 -or
    ($bodyCycles | Measure-Object -Maximum).Maximum -ne 38517 -or
    ($writeCounts | Measure-Object -Minimum).Minimum -ne 11 -or
    ($writeCounts | Measure-Object -Maximum).Maximum -ne 523
) {
    throw "7F57 frame, timing, or write-count range changed."
}
if (
    $calls[0].input_x -ne "62" -or
    [int64] $calls[0].body_master_cycles -ne 417 -or
    [int] $calls[0].write_count -ne 11
) {
    throw "7F57 normalized replay sample changed."
}

$hitWrites = [System.Collections.Generic.List[object]]::new()
$awardThirtyWrites = [System.Collections.Generic.List[object]]::new()
$audioWrites = [System.Collections.Generic.List[object]]::new()
$countdownWrites = [System.Collections.Generic.List[object]]::new()
$holdingLowWrites = [System.Collections.Generic.List[object]]::new()
$holdingHighWrites = [System.Collections.Generic.List[object]]::new()
$awardFiftyWrites = [System.Collections.Generic.List[object]]::new()
$groupCountWrites = [System.Collections.Generic.List[object]]::new()
$scoreWrites = [System.Collections.Generic.List[object]]::new()
$clearWrites = [System.Collections.Generic.List[object]]::new()
foreach ($write in $writes) {
    $bank = $write.instruction_physical_bank
    $pc = $write.instruction_logical_pc
    if ($bank -eq "03" -and $pc -eq "7F67") {
        $hitWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "7F6C") {
        $awardThirtyWrites.Add($write)
    } elseif (
        $bank -eq "45" -and $pc -eq "4BCC" -and
        $write.work_ram_offset -eq "00D4" -and $write.value -eq "21"
    ) {
        $audioWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "7F78") {
        $countdownWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "7F8C") {
        $holdingLowWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "7F91") {
        $holdingHighWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "7F97") {
        $awardFiftyWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "7FAB") {
        $groupCountWrites.Add($write)
    } elseif (
        $bank -eq "00" -and
        @("F587", "F58F", "F597", "F5A5") -contains $pc
    ) {
        $scoreWrites.Add($write)
    } elseif ($bank -eq "01" -and $pc -eq "C581") {
        $offset = [Convert]::ToInt32($write.work_ram_offset, 16)
        if ($offset -ge 0x1000 -and $offset -le 0x1FFF) {
            $clearWrites.Add($write)
        }
    }
}
if (
    $hitWrites.Count -ne 5 -or
    $awardThirtyWrites.Count -ne 5 -or
    $audioWrites.Count -ne 5 -or
    $countdownWrites.Count -ne 5 -or
    $holdingLowWrites.Count -ne 1 -or
    $holdingHighWrites.Count -ne 1 -or
    $awardFiftyWrites.Count -ne 1 -or
    $groupCountWrites.Count -ne 1 -or
    $scoreWrites.Count -ne 24 -or
    $clearWrites.Count -ne 32
) {
    throw "Branching two-hit path write populations changed."
}
if (
    @($awardThirtyWrites | Where-Object {
        $_.work_ram_offset -ne "002F" -or $_.value -ne "1E"
    }).Count -ne 0 -or
    $awardFiftyWrites[0].work_ram_offset -ne "002F" -or
    $awardFiftyWrites[0].value -ne "32"
) {
    throw "Branching two-hit award parameters changed."
}

$hitContexts = for ($index = 0; $index -lt $countdownWrites.Count; ++$index) {
    $countdown = $countdownWrites[$index]
    $sequence = [int] $countdown.call_sequence
    $call = $calls[$sequence - 1]
    if (
        [int] $hitWrites[$index].call_sequence -ne $sequence -or
        [int] $awardThirtyWrites[$index].call_sequence -ne $sequence -or
        [int] $audioWrites[$index].call_sequence -ne $sequence
    ) {
        throw "Hit path writes left their owning 7F57 call."
    }
    [pscustomobject] @{
        Context = (
            "$sequence`:$($call.frame):$($call.input_x):" +
            "$($countdown.value):$($call.return_instruction_pc)"
        )
    }
}
Assert-Histogram $hitContexts "Context" @{
    "3131:1479:62:01:7F80" = 1
    "6652:1832:63:01:7F80" = 1
    "6851:1852:62:00:C594" = 1
    "7774:1955:69:01:7F80" = 1
    "7783:1959:69:00:7F94" = 1
} "Branching two-hit hit"

foreach ($sequence in @("3131", "6652", "7774", "7783")) {
    if (
        @(
            $scoreWrites |
                Where-Object { $_.call_sequence -eq $sequence }
        ).Count -ne 4
    ) {
        throw "Hit call $sequence stopped applying one score award."
    }
}
if (
    @(
        $scoreWrites |
            Where-Object { $_.call_sequence -eq "6851" }
    ).Count -ne 8
) {
    throw "Standard completion stopped applying two score awards."
}
if (
    $holdingLowWrites[0].call_sequence -ne "7783" -or
    $holdingLowWrites[0].work_ram_offset -ne "1F69" -or
    $holdingLowWrites[0].value -ne "81" -or
    $holdingHighWrites[0].call_sequence -ne "7783" -or
    $holdingHighWrites[0].work_ram_offset -ne "1FE9" -or
    $holdingHighWrites[0].value -ne "7F"
) {
    throw "Subtype-0F holding transition changed."
}
if (
    $groupCountWrites[0].call_sequence -ne "6851" -or
    $groupCountWrites[0].work_ram_offset -ne "0D16" -or
    $groupCountWrites[0].value -ne "04"
) {
    throw "Standard completion group-count decrement changed."
}
foreach ($write in $clearWrites) {
    $offset = [Convert]::ToInt32($write.work_ram_offset, 16)
    if (
        $write.call_sequence -ne "6851" -or
        ($offset -band 0x7F) -ne 0x62 -or
        $write.value -ne "00"
    ) {
        throw "Standard completion stopped clearing slot 62."
    }
}

# The actor identity artifact proves ten allocations, their initial 7EFA
# state, the transition to 7F57, and the subtype-$0F holding-state writes.
$spawnTypeWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7E8F"
        }
)
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
$expectedSpawnFrames = "873,873,988,988,1036,1036,1084,1084,1132,1132"
$expectedTypeAddresses = "3062,3063,3065,3066,3068,3069,306B,306C,306E,306F"
$expectedLowAddresses = "3F62,3F63,3F65,3F66,3F68,3F69,3F6B,3F6C,3F6E,3F6F"
$expectedHighAddresses = "3FE2,3FE3,3FE5,3FE6,3FE8,3FE9,3FEB,3FEC,3FEE,3FEF"
if (
    ($spawnTypeWrites.frame -join ",") -ne $expectedSpawnFrames -or
    ($spawnTypeWrites.logical_address -join ",") -ne
        $expectedTypeAddresses -or
    ($spawnTypeWrites.value -join ",") -ne
        "82,82,82,82,82,82,82,82,82,82" -or
    ($spawnLowWrites.logical_address -join ",") -ne
        $expectedLowAddresses -or
    ($spawnLowWrites.value -join ",") -ne
        "FA,FA,FA,FA,FA,FA,FA,FA,FA,FA" -or
    ($spawnHighWrites.logical_address -join ",") -ne
        $expectedHighAddresses -or
    ($spawnHighWrites.value -join ",") -ne
        "7E,7E,7E,7E,7E,7E,7E,7E,7E,7E"
) {
    throw "Branching two-hit allocation identity changed."
}

$steadyLowWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            $expectedLowAddresses.Split(",") -contains
                $_.logical_address -and
            $_.value -eq "57"
        }
)
$steadyHighWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6" -and
            $expectedHighAddresses.Split(",") -contains
                $_.logical_address -and
            $_.value -eq "7F" -and
            $steadyLowWrites.frame -contains $_.frame
        }
)
$expectedSteadyFrames = "1053,1085,1101,1133,1149,1181,1197,1229,1245,1277"
if (
    ($steadyLowWrites.frame -join ",") -ne $expectedSteadyFrames -or
    ($steadyHighWrites.frame -join ",") -ne $expectedSteadyFrames
) {
    throw "Branching two-hit steady-state transition changed."
}

$holdingStateLowWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7F8C"
        }
)
$holdingStateHighWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "7F91"
        }
)
if (
    ($holdingStateLowWrites.frame -join ",") -ne
        "1959,1960,1961,1962,1963,1964,1965,1969" -or
    ($holdingStateHighWrites.frame -join ",") -ne
        "1959,1960,1961,1962,1963,1964,1965,1969" -or
    @($holdingStateLowWrites | Where-Object {
        $_.logical_address -ne "3F69" -or $_.value -ne "81"
    }).Count -ne 0 -or
    @($holdingStateHighWrites | Where-Object {
        $_.logical_address -ne "3FE9" -or $_.value -ne "7F"
    }).Count -ne 0
) {
    throw "Subtype-0F holding-state persistence changed."
}

Write-Output "Branching two-hit semantics verified."
Write-Output (
    "7F57: 20867 calls update 10 slots; 20862 are no-hit paths."
)
Write-Output (
    "Hit rule: 5 hits award 30 and emit command 21."
)
Write-Output (
    "Second-hit branches: slot 62 adds 50 and is freed; subtype-0F slot " +
    "69 enters holding dispatch 7F81."
)
Write-Output (
    "Score contribution: 200 stored units; slot 63 remains at one hit."
)
Write-Output (
    "First normalized no-hit sample: 03:7F57@1, frame 1054, " +
    "417 cycles, 11 writes."
)
