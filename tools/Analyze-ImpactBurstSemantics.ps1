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

function Get-LastValueBeforeCycle {
    param(
        [hashtable] $WritesByAddress,
        [string] $Address,
        [int64] $Cycle
    )

    if (-not $WritesByAddress.ContainsKey($Address)) {
        return "--"
    }
    $last = @(
        $WritesByAddress[$Address] |
            Where-Object {
                [int64] $_.absolute_master_cycle -lt $Cycle
            } |
            Select-Object -Last 1
    )
    if ($last.Count -eq 0) {
        return "--"
    }
    return $last[0].value
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# 41EC is the default completion-effect entry: it clears positional offsets,
# selects command $2D, and falls through the common allocator. 41FF is the
# alternate entry used after a source has selected another command.
Assert-RomBytes $rom 0x0041EC @(
    0x64, 0x2B, 0x64, 0x2C,
    0xA9, 0x2D, 0x8D, 0x6E, 0x2D,
    0xA9, 0xE1, 0x85, 0x2D,
    0xA9, 0x42, 0x85, 0x2E,
    0x80, 0x0C
) "Default impact-burst entry"
Assert-RomBytes $rom 0x0041FF @(
    0x64, 0x2B, 0x64, 0x2C,
    0xA9, 0xE1, 0x85, 0x2D,
    0xA9, 0x42, 0x85, 0x2E
) "Alternate impact-burst entry"

# The common allocator copies the source position into a newly allocated
# slot, creates type $8F, installs dispatch 4290, and initializes the effect
# timer/state. It does not award score.
Assert-RomBytes $rom 0x00420B @(
    0x20, 0x24, 0xC0, 0xB0, 0x6D,
    0xA5, 0x2D, 0x99, 0x00, 0x3E,
    0xA5, 0x2E, 0x99, 0x80, 0x3E,
    0x64, 0x26,
    0xA5, 0x2B, 0x10, 0x02, 0xC6, 0x26,
    0x18, 0xBD, 0x80, 0x31, 0x65, 0x2B, 0x99, 0x80, 0x31,
    0xBD, 0x00, 0x32, 0x65, 0x26, 0x99, 0x00, 0x32,
    0x64, 0x26,
    0xA5, 0x2C, 0x10, 0x02, 0xC6, 0x26,
    0x18, 0xBD, 0x00, 0x33, 0x65, 0x2C, 0x99, 0x00, 0x33,
    0xBD, 0x80, 0x33, 0x65, 0x26, 0x99, 0x80, 0x33,
    0x62, 0x99, 0x00, 0x38,
    0x99, 0x80, 0x39,
    0xB9, 0x00, 0x30, 0x29, 0xF0, 0x09, 0x0F, 0x99, 0x00, 0x30,
    0xA9, 0x90, 0x99, 0x00, 0x3F,
    0xA9, 0x42, 0x99, 0x80, 0x3F,
    0xA9, 0x04, 0x99, 0x00, 0x3A,
    0xA9, 0x10, 0x99, 0x00, 0x39,
    0xBD, 0x80, 0x30, 0x29, 0x80, 0x09, 0x64, 0x99, 0x80, 0x30,
    0x18, 0x60, 0x38, 0x60
) "Impact-burst common allocator"

# 4290 is the single activation call. It clears the pending flag, emits the
# source-selected command from $2D6E, and transitions to the ten-step effect
# at 42B0. That state frees the slot after table value $FF.
Assert-RomBytes $rom 0x004290 @(
    0xBD, 0x80, 0x30, 0x30, 0x03, 0x4C, 0x27, 0xC0,
    0xBD, 0x80, 0x30, 0x29, 0xDF, 0x9D, 0x80, 0x30,
    0xAD, 0x6E, 0x2D, 0x20, 0x87, 0xC0,
    0xA9, 0xB0, 0x9D, 0x00, 0x3F,
    0xA9, 0x42, 0x9D, 0x80, 0x3F
) "Impact-burst activation"
Assert-RomBytes $rom 0x0042B0 @(
    0xBD, 0x80, 0x30, 0x30, 0x03, 0x4C, 0x27, 0xC0,
    0xDE, 0x00, 0x3A, 0xD0, 0x23,
    0xA9, 0x04, 0x9D, 0x00, 0x3A,
    0xBC, 0x80, 0x3A, 0xC8, 0x98, 0x9D, 0x80, 0x3A,
    0xBD, 0x00, 0x3E, 0x85, 0x2F,
    0xBD, 0x80, 0x3E, 0x85, 0x30,
    0xB1, 0x2F, 0xC9, 0xFF, 0xD0, 0x03, 0x4C, 0x27, 0xC0,
    0x9D, 0x80, 0x39, 0x60,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF
) "Impact-burst ten-step animation"

# Known source rules prove this is shared effect glue rather than an
# independent table mechanism: Small Soldiers and Tower insects use the
# default entry, while two other sources select commands $39 and $34.
Assert-RomBytes $rom 0x00603F @(
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x63, 0xC0, 0x20, 0xEC, 0x41,
    0xCE, 0x2B, 0x2D, 0x4C, 0x27, 0xC0
) "Tower-insect completion source"
Assert-RomBytes $rom 0x007F95 @(
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x63, 0xC0, 0x20, 0xEC, 0x41
) "Small-Soldier completion source"
Assert-RomBytes $rom 0x00435F @(
    0xA9, 0x39, 0x8D, 0x6E, 0x2D,
    0xA9, 0xF8, 0x85, 0x2B,
    0xA9, 0xF8, 0x85, 0x2C,
    0x20, 0xFF, 0x41
) "Command-39 impact source"
Assert-RomBytes $rom 0x004443 @(
    0x20, 0x63, 0xC0,
    0xA9, 0x34, 0x8D, 0x6E, 0x2D,
    0x20, 0xFF, 0x41, 0x4C, 0x27, 0xC0
) "Command-34 impact source"
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 14 -or
    $writes.Count -ne 1231 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 14 impact-burst calls, 1,231 scoped writes, and " +
        "3,872 actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("41EC", "41FF", "4290") -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Impact-burst bounded-call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Impact burst changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "41EC" = 5
    "41FF" = 2
    "4290" = 7
} "Impact-burst routine"
Assert-Histogram $calls "return_instruction_pc" @{
    "427C" = 7
    "42E0" = 7
} "Impact-burst return"

$spawnCalls = @($calls | Where-Object { $_.logical_pc -ne "4290" })
$impactCalls = @($calls | Where-Object { $_.logical_pc -eq "4290" })
$expectedSpawnSequences = @(1, 3, 5, 7, 9, 11, 13)
$expectedSpawnFrames = @(1852, 1970, 2078, 2425, 2429, 2595, 2629)
$expectedEntries = @("41EC", "41EC", "41EC", "41FF", "41FF", "41EC", "41EC")
$expectedCallers = @("7FA1", "7FA1", "604B", "436C", "444B", "604B", "604B")
$expectedSources = @("62", "69", "64", "22", "64", "6A", "6D")
$expectedDestinations = @("70", "62", "71", "62", "69", "62", "69")
$expectedImpactFrames = @(1852, 1970, 2078, 2425, 2429, 2596, 2630)
$expectedCommands = @("2D", "2D", "2D", "39", "34", "2D", "2D")

for ($index = 0; $index -lt 7; ++$index) {
    $spawn = $spawnCalls[$index]
    $impact = $impactCalls[$index]
    if (
        [int] $spawn.sequence -ne $expectedSpawnSequences[$index] -or
        [int] $spawn.frame -ne $expectedSpawnFrames[$index] -or
        $spawn.logical_pc -ne $expectedEntries[$index] -or
        $spawn.caller_pc -ne $expectedCallers[$index] -or
        $spawn.input_x -ne $expectedSources[$index] -or
        [int] $impact.frame -ne $expectedImpactFrames[$index] -or
        $impact.input_x -ne $expectedDestinations[$index]
    ) {
        throw "Impact-burst spawn/activation pair $($index + 1) changed."
    }

    $spawnWrites = @(
        $writes |
            Where-Object { [int] $_.call_sequence -eq [int] $spawn.sequence }
    )
    $destination = [Convert]::ToInt32($expectedDestinations[$index], 16)
    $pointerLowOffset = "1F{0:X2}" -f $destination
    $pointerHighOffset = "1F{0:X2}" -f ($destination + 0x80)
    $typeWrites = @(
        $spawnWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -eq "425A" -and
                $_.work_ram_offset -eq "10$($expectedDestinations[$index])" -and
                $_.value -eq "8F"
            }
    )
    $initialPointers = @(
        $spawnWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -in @("425F", "4264") -and
                $_.work_ram_offset -in @($pointerLowOffset, $pointerHighOffset)
            }
    )
    if (
        $typeWrites.Count -ne 1 -or
        ($initialPointers.value -join ",") -ne "90,42"
    ) {
        throw "Impact-burst allocation $($index + 1) changed."
    }

    $impactWrites = @(
        $writes |
            Where-Object { [int] $_.call_sequence -eq [int] $impact.sequence }
    )
    $audio = @(
        $impactWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq "ECFD" -and
                $_.work_ram_offset -eq "002A" -and
                $_.value -eq $expectedCommands[$index]
            }
    )
    $steadyPointers = @(
        $impactWrites |
            Where-Object {
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -in @("42A8", "42AD")
            }
    )
    if (
        $audio.Count -ne 1 -or
        ($steadyPointers.value -join ",") -ne "B0,42"
    ) {
        throw "Impact-burst activation $($index + 1) changed."
    }
}

Assert-Histogram (
    @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq "ECFD" -and
                $_.work_ram_offset -eq "002A"
            }
    )
) "value" @{
    "2D" = 5
    "34" = 1
    "39" = 1
} "Impact-burst audio command"

$scoreWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -in @("F587", "F58F", "F597", "F5A5")
        }
)
if ($scoreWrites.Count -ne 0) {
    throw "Impact-burst glue unexpectedly changed score."
}

$writesByAddress = $actorWrites |
    Group-Object -Property logical_address -AsHashTable -AsString
$expectedSourceDispatches = @(
    "7F57", "7F81", "6016", "4345", "441E", "6016", "6016"
)
for ($index = 0; $index -lt 7; ++$index) {
    $call = $spawnCalls[$index]
    $slot = $expectedSources[$index]
    $cycle = [int64] $call.entry_absolute_master_cycle
    $type = Get-LastValueBeforeCycle $writesByAddress "30$slot" $cycle
    $low = Get-LastValueBeforeCycle $writesByAddress "3F$slot" $cycle
    $highAddress = "3F{0:X2}" -f (
        [Convert]::ToInt32($slot, 16) + 0x80
    )
    $high = Get-LastValueBeforeCycle $writesByAddress $highAddress $cycle
    if (
        $type -ne "82" -or
        "$high$low" -ne $expectedSourceDispatches[$index]
    ) {
        throw "Impact-burst source context $($index + 1) changed."
    }
}

$expectedFreeFrames = @(1888, 2005, 2113, 2460, 2464, 2631, 2665)
for ($index = 0; $index -lt 7; ++$index) {
    $slot = $expectedDestinations[$index]
    $allocationCycle = [int64] $spawnCalls[$index].entry_absolute_master_cycle
    $clear = @(
        $actorWrites |
            Where-Object {
                $_.logical_address -eq "30$slot" -and
                $_.value -eq "00" -and
                [int64] $_.absolute_master_cycle -gt $allocationCycle
            } |
            Select-Object -First 1
    )
    if (
        $clear.Count -ne 1 -or
        [int] $clear[0].frame -ne $expectedFreeFrames[$index] -or
        $clear[0].instruction_physical_bank -ne "01" -or
        $clear[0].instruction_logical_pc -ne "C581"
    ) {
        throw "Impact-burst lifetime $($index + 1) changed."
    }
}

if (
    [int64] $impactCalls[0].body_master_cycles -ne 4362 -or
    [int] $impactCalls[0].write_count -ne 114
) {
    throw "First normalized impact-burst sample changed."
}

Write-Output "Impact-burst semantics verified."
Write-Output (
    "Shared sources: Small Soldiers x2, Tower insects x3, " +
    "Dragon's Egg slot 22 x1, and hatchling 02:441E x1."
)
Write-Output (
    "Seven type-8F effects: source position copy, one 02:4290 activation, " +
    "ten-step animation, then slot release."
)
Write-Output (
    "Source-selected feedback: command 2D x5, 34 x1, 39 x1; " +
    "the effect itself awards no score."
)
Write-Output (
    "First normalized sample: 02:4290@1, frame 1852, 4362 cycles, " +
    "114 writes."
)
