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

# The runtime spawner claims a generic slot, assigns type $82, copies source
# position/collision fields, initializes a two-hit counter, installs 02:5FDB,
# refreshes activation, and increments the live-instance count at $2D2B.
Assert-RomBytes $rom 0x005F78 @(
    0x20, 0x24, 0xC0, 0x90, 0x01, 0x60,
    0xA9, 0x82, 0x99, 0x00, 0x30,
    0xBD, 0x80, 0x31, 0x99, 0x80, 0x31,
    0xBD, 0x00, 0x32, 0x99, 0x00, 0x32,
    0xBD, 0x00, 0x33, 0x99, 0x00, 0x33,
    0xBD, 0x80, 0x33, 0x99, 0x80, 0x33,
    0xBD, 0x00, 0x39, 0x99, 0x00, 0x39,
    0xA9, 0x80, 0x99, 0x80, 0x30,
    0xA9, 0x10, 0x99, 0x80, 0x3D,
    0xBD, 0x00, 0x37, 0x99, 0x00, 0x37,
    0xBD, 0x80, 0x37, 0x99, 0x80, 0x37,
    0xA9, 0x10, 0x99, 0x00, 0x38,
    0xBD, 0x00, 0x3D, 0x99, 0x00, 0x3D,
    0xA9, 0x02, 0x99, 0x00, 0x3C,
    0xA9, 0xDB, 0x99, 0x00, 0x3F,
    0xA9, 0x5F, 0x99, 0x80, 0x3F,
    0xDA, 0x02, 0x20, 0x6C, 0xC0, 0xFA,
    0xEE, 0x2B, 0x2D, 0x60
) "5F78 dynamic two-hit target spawner"

# The steady state begins in raw bank 03 after the initial 02:5FDB animation.
# A collision result sets bit 7 at $3800,X. The hit path clears it, awards 10,
# emits command $29, and decrements $3C00,X. Reaching zero falls through to a
# 50-unit completion award, common completion helper, and generic slot clear.
Assert-RomBytes $rom 0x006016 @(
    0xBD, 0x80, 0x37, 0x20, 0x66, 0x69, 0x44, 0x36,
    0xBD, 0x00, 0x38, 0x10, 0x1B,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xA9, 0x0A, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x29, 0x20, 0x87, 0xC0,
    0xDE, 0x00, 0x3C, 0xF0, 0x04, 0x20, 0xF6, 0x7B, 0x60,
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x63, 0xC0, 0x20, 0xEC, 0x41,
    0xCE, 0x2B, 0x2D, 0x4C, 0x27, 0xC0
) "6016 two-hit target update"
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
    $calls.Count -ne 6494 -or
    $writes.Count -ne 114100 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 6,494 calls, 114,100 scoped writes, and 3,872 actor " +
        "identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne "6016" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "6016 bounded-call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "6016 changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "input_x" @{
    "64" = 1049
    "67" = 2519
    "6A" = 1470
    "6D" = 1456
} "6016 slot"
Assert-Histogram $calls "return_instruction_pc" @{
    "603E" = 6491
    "C594" = 3
} "6016 outer return"

$bodyCycles = @($calls | ForEach-Object { [int64] $_.body_master_cycles })
$writeCounts = @($calls | ForEach-Object { [int] $_.write_count })
if (
    [int] $calls[0].frame -ne 1023 -or
    [int] $calls[-1].frame -ne 3600 -or
    ($bodyCycles | Measure-Object -Minimum).Minimum -ne 441 -or
    ($bodyCycles | Measure-Object -Maximum).Maximum -ne 27474 -or
    ($writeCounts | Measure-Object -Minimum).Minimum -ne 11 -or
    ($writeCounts | Measure-Object -Maximum).Maximum -ne 545
) {
    throw "6016 frame, timing, or write-count range changed."
}
$replayCall = $calls[4]
if (
    [int] $replayCall.sequence -ne 5 -or
    [int] $replayCall.frame -ne 1027 -or
    $replayCall.input_x -ne "64" -or
    [int64] $replayCall.body_master_cycles -ne 441 -or
    [int] $replayCall.write_count -ne 11
) {
    throw "6016 normalized replay sample changed."
}

$hitWrites = [System.Collections.Generic.List[object]]::new()
$awardTenWrites = [System.Collections.Generic.List[object]]::new()
$audioWrites = [System.Collections.Generic.List[object]]::new()
$countdownWrites = [System.Collections.Generic.List[object]]::new()
$awardFiftyWrites = [System.Collections.Generic.List[object]]::new()
$liveCountWrites = [System.Collections.Generic.List[object]]::new()
$scoreWrites = [System.Collections.Generic.List[object]]::new()
$clearWrites = [System.Collections.Generic.List[object]]::new()
foreach ($write in $writes) {
    $bank = $write.instruction_physical_bank
    $pc = $write.instruction_logical_pc
    if ($bank -eq "03" -and $pc -eq "6025") {
        $hitWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "602A") {
        $awardTenWrites.Add($write)
    } elseif (
        $bank -eq "45" -and $pc -eq "4BCC" -and
        $write.work_ram_offset -eq "00D4" -and $write.value -eq "29"
    ) {
        $audioWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "6036") {
        $countdownWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "6041") {
        $awardFiftyWrites.Add($write)
    } elseif ($bank -eq "03" -and $pc -eq "604E") {
        $liveCountWrites.Add($write)
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
    $hitWrites.Count -ne 7 -or
    $awardTenWrites.Count -ne 7 -or
    $audioWrites.Count -ne 7 -or
    $countdownWrites.Count -ne 7 -or
    $awardFiftyWrites.Count -ne 3 -or
    $liveCountWrites.Count -ne 3 -or
    $scoreWrites.Count -ne 40 -or
    $clearWrites.Count -ne 96
) {
    throw "Two-hit target path write populations changed."
}
if (
    @($awardTenWrites | Where-Object {
        $_.work_ram_offset -ne "002F" -or $_.value -ne "0A"
    }).Count -ne 0 -or
    @($awardFiftyWrites | Where-Object {
        $_.work_ram_offset -ne "002F" -or $_.value -ne "32"
    }).Count -ne 0
) {
    throw "Two-hit target award parameters changed."
}

$hitContexts = for ($index = 0; $index -lt $countdownWrites.Count; ++$index) {
    $countdown = $countdownWrites[$index]
    $sequence = [int] $countdown.call_sequence
    $call = $calls[$sequence - 1]
    if (
        [int] $hitWrites[$index].call_sequence -ne $sequence -or
        [int] $awardTenWrites[$index].call_sequence -ne $sequence -or
        [int] $audioWrites[$index].call_sequence -ne $sequence
    ) {
        throw "Hit path writes left their owning 6016 call."
    }
    [pscustomobject] @{
        Context = (
            "$sequence`:$($call.frame):$($call.input_x):" +
            "$($countdown.value):$($call.return_instruction_pc)"
        )
    }
}
Assert-Histogram $hitContexts "Context" @{
    "1260:1409:6D:01:603E" = 1
    "3111:1875:6A:01:603E" = 1
    "3889:2074:64:01:603E" = 1
    "3905:2078:64:00:C594" = 1
    "5458:2595:6A:00:C594" = 1
    "5527:2629:6D:00:C594" = 1
    "5829:2931:67:01:603E" = 1
} "Two-hit target hit"

$terminalSequences = @("3905", "5458", "5527")
for ($index = 0; $index -lt $terminalSequences.Count; ++$index) {
    $sequence = $terminalSequences[$index]
    $slot = @("64", "6A", "6D")[$index]
    $terminalClears = @(
        $clearWrites |
            Where-Object { $_.call_sequence -eq $sequence }
    )
    if ($terminalClears.Count -ne 32) {
        throw "Terminal call $sequence changed its 32-array clear."
    }
    foreach ($write in $terminalClears) {
        $offset = [Convert]::ToInt32($write.work_ram_offset, 16)
        if (
            ("{0:X2}" -f ($offset -band 0x7F)) -ne $slot -or
            $write.value -ne "00"
        ) {
            throw "Terminal call $sequence cleared the wrong slot."
        }
    }
    $terminalScoreWrites = @(
        $scoreWrites |
            Where-Object { $_.call_sequence -eq $sequence }
    )
    if ($terminalScoreWrites.Count -ne 8) {
        throw "Terminal call $sequence stopped applying two score awards."
    }
}
foreach ($sequence in @("1260", "3111", "3889", "5829")) {
    if (
        @(
            $scoreWrites |
                Where-Object { $_.call_sequence -eq $sequence }
        ).Count -ne 4
    ) {
        throw "Nonterminal hit $sequence stopped applying one score award."
    }
}
if (
    ($liveCountWrites.value -join ",") -ne "03,02,01" -or
    ($liveCountWrites.work_ram_offset -join ",") -ne "0D2B,0D2B,0D2B"
) {
    throw "Two-hit target live-instance count changed."
}

# The global actor identity trace proves four allocations and their transition
# from initial state $5FDB to steady update $6016.
$spawnTypeWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5F80"
        }
)
$spawnLowWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5FC9"
        }
)
$spawnHighWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5FCE"
        }
)
if (
    ($spawnTypeWrites.frame -join ",") -ne "941,989,1037,1085" -or
    ($spawnTypeWrites.logical_address -join ",") -ne
        "3064,3067,306A,306D" -or
    ($spawnTypeWrites.value -join ",") -ne "82,82,82,82" -or
    ($spawnLowWrites.logical_address -join ",") -ne
        "3F64,3F67,3F6A,3F6D" -or
    ($spawnLowWrites.value -join ",") -ne "DB,DB,DB,DB" -or
    ($spawnHighWrites.logical_address -join ",") -ne
        "3FE4,3FE7,3FEA,3FED" -or
    ($spawnHighWrites.value -join ",") -ne "5F,5F,5F,5F"
) {
    throw "Two-hit target allocation identity changed."
}

$steadyLowWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            @("3F64", "3F67", "3F6A", "3F6D") -contains
                $_.logical_address -and
            $_.value -eq "16"
        }
)
$steadyHighWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6" -and
            @("3FE4", "3FE7", "3FEA", "3FED") -contains
                $_.logical_address -and
            $_.value -eq "60"
        }
)
if (
    ($steadyLowWrites.frame -join ",") -ne "1022,1070,1118,1166" -or
    ($steadyHighWrites.frame -join ",") -ne "1022,1070,1118,1166"
) {
    throw "Two-hit target steady-state transition changed."
}

Write-Output "Two-hit target semantics verified."
Write-Output (
    "6016: 6494 calls update slots 64/67/6A/6D; 6487 are no-hit paths."
)
Write-Output (
    "Hit rule: 7 hits award 10 and emit command 29; 3 second hits also " +
    "award 50 and free the slot."
)
Write-Output (
    "Score contribution: 220 stored units; completed slots 64,6A,6D; " +
    "slot 67 remains at one hit."
)
Write-Output (
    "First normalized no-hit sample: 03:6016@5, frame 1027, " +
    "441 cycles, 11 writes."
)
