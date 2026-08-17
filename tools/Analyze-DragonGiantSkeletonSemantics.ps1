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

function Assert-SingleWrite {
    param(
        [object[]] $Rows,
        [string] $Bank,
        [string] $Pc,
        [string] $Offset,
        [string] $Value,
        [string] $Label
    )

    $matches = @(
        $Rows |
            Where-Object {
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($matches.Count -ne 1) {
        throw "$Label changed: expected one matching write, found $($matches.Count)."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# These records are the lower-left Dragon's Head and lower-right Giant
# Skeleton in the USA manual. Player documentation calls the latter Big Skull.
Assert-RomBytes $rom 0x004647 @(
    0x00, 0x13, 0x31, 0x5A, 0x40, 0x00,
    0xFE, 0x01, 0x0C, 0x8A, 0x82, 0x00
) "Dragon's-Head layout record"
Assert-RomBytes $rom 0x0047A3 @(
    0x00, 0x30, 0xE4, 0x5E, 0xE0, 0x00,
    0x06, 0x02, 0x1F, 0x82, 0x83, 0x00
) "Giant-Skeleton layout record"

# The Dragon initializes an eight-hit counter. Each hit awards 50 stored
# units, emits command $2E, applies the standard bonus/appearance helpers,
# and decrements the counter. Counter zero falls into a second 50-unit award
# and the mouth-opening transition.
Assert-RomBytes $rom 0x005A31 @(
    0xBD, 0x80, 0x30, 0x29, 0xBF, 0x9D, 0x80, 0x30,
    0xA9, 0x10, 0x9D, 0x00, 0x38,
    0xA9, 0x82, 0x9D, 0x00, 0x3D,
    0xA9, 0x08, 0x9D, 0x00, 0x3C,
    0x20, 0x18, 0xC0
) "Dragon's-Head initializer"
Assert-RomBytes $rom 0x005A4B @(
    0xBD, 0x00, 0x38, 0x10, 0x23,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x2E, 0x20, 0x87, 0xC0,
    0x20, 0x69, 0xC0, 0x20, 0xF6, 0x7B,
    0xA9, 0x03, 0x20, 0x43, 0x7C,
    0xDE, 0x00, 0x3C, 0xF0, 0x01, 0x60
) "Dragon's-Head hit rule"
Assert-RomBytes $rom 0x005A74 @(
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x01, 0x9D, 0x00, 0x3A, 0x20, 0x18, 0xC0,
    0xDE, 0x00, 0x3A, 0xD0, 0x0F,
    0xA9, 0x04, 0x9D, 0x00, 0x3A,
    0xFE, 0x80, 0x39, 0xBD, 0x80, 0x39, 0xC9, 0x04,
    0xF0, 0x01, 0x60
) "Dragon's-Head terminal opening"

# The Giant Skeleton chooses one of five appearance values from the shared
# RNG. Its repeatable hit clears the collision latch, awards 40 stored units,
# emits the same command $2E, and leaves the actor alive.
Assert-RomBytes $rom 0x005EE4 @(
    0xA9, 0x03, 0x9D, 0x80, 0x39, 0x20, 0x18, 0xC0
) "Giant-Skeleton initializer"
Assert-RomBytes $rom 0x005EEC @(
    0x20, 0x6F, 0xC0, 0xC9, 0x04, 0xB0, 0x04,
    0xA9, 0x04, 0x80, 0x06,
    0xC9, 0x0D, 0x90, 0x02, 0xA9, 0x0C,
    0x4A, 0x9D, 0x80, 0x39,
    0xBD, 0x00, 0x38, 0x10, 0x1B,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xA9, 0x02, 0x20, 0x43, 0x7C,
    0xA9, 0x28, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x2E, 0x20, 0x87, 0xC0,
    0x20, 0x69, 0xC0, 0x60
) "Giant-Skeleton repeatable hit rule"
Assert-RomBytes $rom 0x00202A @(0x4C, 0x4F, 0xF5) "Score entry"
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 4283 -or
    $writes.Count -ne 50711 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 4,283 Dragon/Giant-Skeleton calls, 50,711 scoped writes, " +
        "and 3,872 actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("5A4B", "5EEC") -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Lower-table bounded-call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in @(0, 1, 2, 3, 6, 7)) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Lower-table actor changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "5A4B" = 2144
    "5EEC" = 2139
} "Lower-table routine"
Assert-Histogram $calls "input_x" @{
    "13" = 2144
    "30" = 2139
} "Lower-table fixed slot"

$dragonCalls = @($calls | Where-Object { $_.logical_pc -eq "5A4B" })
$skeletonCalls = @($calls | Where-Object { $_.logical_pc -eq "5EEC" })
if (
    [int] $dragonCalls[0].frame -ne 872 -or
    [int] $dragonCalls[-1].frame -ne 3600 -or
    [int] $skeletonCalls[0].frame -ne 873 -or
    [int] $skeletonCalls[-1].frame -ne 3600 -or
    @($dragonCalls | Where-Object {
        $_.body_master_cycles -eq "27" -and $_.write_count -eq "0"
    }).Count -ne 2125 -or
    [int64] $dragonCalls[0].body_master_cycles -ne 27 -or
    [int] $dragonCalls[0].write_count -ne 0
) {
    throw "Lower-table frame, idle-path, or replay sample changed."
}

$skeletonAnimations = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5EFE" -and
            $_.work_ram_offset -eq "19B0"
        }
)
Assert-Histogram $skeletonAnimations "value" @{
    "02" = 776
    "03" = 704
    "04" = 140
    "05" = 184
    "06" = 335
} "Giant-Skeleton RNG appearance"

$dragonHitMarkers = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5A57" -and
            $_.work_ram_offset -eq "002F" -and
            $_.value -eq "32"
        }
)
$skeletonHitMarkers = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5F12" -and
            $_.work_ram_offset -eq "002F" -and
            $_.value -eq "28"
        }
)
if (
    ($dragonHitMarkers.call_sequence -join ",") -ne "1242,1250" -or
    ($skeletonHitMarkers.call_sequence -join ",") -ne "2375" -or
    [int] $calls[1241].frame -ne 2076 -or
    [int] $calls[1249].frame -ne 2080 -or
    [int] $calls[2374].frame -ne 2642
) {
    throw "Dragon or Giant-Skeleton hit chronology changed."
}

$expectedHitSequences = @(1242, 1250, 2375)
$expectedScoreLowValues = @("4A", "B8", "DE")
foreach ($index in 0..2) {
    $sequence = $expectedHitSequences[$index]
    $callWrites = @(
        $writes |
            Where-Object { [int] $_.call_sequence -eq $sequence }
    )
    Assert-SingleWrite (
        $callWrites
    ) "00" "ECFD" "002A" "2E" "Shared hit audio command"
    Assert-SingleWrite (
        $callWrites
    ) "00" "F587" "0205" $expectedScoreLowValues[$index] (
        "Selected score low byte"
    )
    Assert-SingleWrite (
        $callWrites
    ) "00" "F5A5" "0CC0" "01" "Score presentation dirty flag"
}

$dragonCounterWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5A6E" -and
            $_.work_ram_offset -eq "1C13"
        }
)
if (
    ($dragonCounterWrites.call_sequence -join ",") -ne "1242,1250" -or
    ($dragonCounterWrites.value -join ",") -ne "07,06"
) {
    throw "Dragon's-Head eight-hit counter changed."
}

$terminalWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5A76"
        }
)
if ($terminalWrites.Count -ne 0) {
    throw "Dragon's-Head terminal opening unexpectedly became active."
}

$audioWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "ECFD" -and
            $_.work_ram_offset -eq "002A"
        }
)
if (
    $audioWrites.Count -ne 3 -or
    ($audioWrites.call_sequence -join ",") -ne "1242,1250,2375" -or
    @($audioWrites | Where-Object { $_.value -ne "2E" }).Count -ne 0
) {
    throw "Shared command-2E ownership changed."
}

$identityWrites = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @(
                "3013", "3F13", "3F93", "3030", "3F30", "3FB0"
            ) -and
            [int] $_.frame -ge 871
        }
)
$identityTimeline = @(
    $identityWrites |
        ForEach-Object {
            "$($_.frame):$($_.logical_address)=$($_.value)"
        }
) -join ","
$expectedIdentityTimeline = @(
    "871:3013=00", "871:3F13=00", "871:3F93=00",
    "871:3013=82", "871:3F13=4B", "871:3F93=5A",
    "872:3030=00", "872:3F30=00", "872:3FB0=00",
    "872:3030=83", "872:3F30=EC", "872:3FB0=5E"
) -join ","
if ($identityTimeline -ne $expectedIdentityTimeline) {
    throw "Dragon/Giant-Skeleton fixed identity changed."
}

Write-Output "Dragon/Giant-Skeleton semantics verified."
Write-Output (
    "Dragon's Head: slot 13 at (64,510), two hits at frames 2076/2080 " +
    "award 50 each and advance the eight-hit counter 8->7->6."
)
Write-Output (
    "Giant Skeleton: slot 30 at (224,518), frame-2642 hit awards 40; " +
    "all 2139 calls retain its repeatable live state."
)
Write-Output (
    "Shared command 2E: three observed hits contribute 140 stored score " +
    "units; the Dragon terminal opening remains inactive."
)
Write-Output (
    "First normalized sample: 02:5A4B@1, frame 872, 27 cycles, 0 writes."
)
Write-Output (
    "Native Giant-Skeleton sample: 02:5EEC@1, frame 873, 807 cycles, " +
    "19 writes."
)
