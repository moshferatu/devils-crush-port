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

function Assert-Write {
    param(
        [object[]] $Writes,
        [string] $InstructionBank,
        [string] $InstructionPc,
        [string] $Offset,
        [string] $Value,
        [string] $Label
    )

    $matches = @(
        $Writes |
            Where-Object {
                $_.instruction_physical_bank -eq $InstructionBank -and
                $_.instruction_logical_pc -eq $InstructionPc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($matches.Count -ne 1) {
        throw "$Label changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The official manual calls these neighboring lower-table mechanisms the
# Tower and the insects living in the Tower. The fixed layout establishes the
# stable identity independently of the shared actor type byte:
#   slot $2C: insect emitter, category 2, 03:5F22, (140,526), type $82
#   slot $2D: Tower, category 0, 03:60B3, (144,538), type $83
Assert-RomBytes $rom 0x004773 @(
    0x02, 0x2C, 0x22, 0x5F, 0x8C, 0x00,
    0x0E, 0x02, 0x39, 0xE0, 0x82, 0x00
) "Tower-insect layout record"
Assert-RomBytes $rom 0x00477F @(
    0x00, 0x2D, 0xB3, 0x60, 0x90, 0x00,
    0x1A, 0x02, 0x18, 0xA0, 0x83, 0x00
) "Tower layout record"

# The insect source installs collision parameters, clears the live-child
# count at $2D2B, and initializes a four-spawn countdown. Its steady state at
# 5F4A uses BSR $5F78 to create the children. Once no children remain, the
# source declares the 200-unit (20,000-point) group-clear award.
Assert-RomBytes $rom 0x005F22 @(
    0x20, 0x18, 0xC0, 0xAD, 0x2D, 0x2D, 0xF0, 0x01, 0x60,
    0xA9, 0x24, 0x9D, 0x00, 0x37,
    0xA9, 0x25, 0x9D, 0x80, 0x37,
    0xA9, 0x8C, 0x9D, 0x00, 0x3D,
    0x9C, 0x2B, 0x2D,
    0xA9, 0x01, 0x9D, 0x00, 0x3A,
    0xA9, 0x04, 0x9D, 0x00, 0x3B,
    0x20, 0x18, 0xC0,
    0xDE, 0x00, 0x3A, 0xD0, 0x0C,
    0xA9, 0x30, 0x9D, 0x00, 0x3A,
    0x44, 0x22,
    0xDE, 0x00, 0x3B, 0xF0, 0x01, 0x60,
    0x20, 0x18, 0xC0,
    0xAD, 0x2B, 0x2D, 0xF0, 0x01, 0x60,
    0xA9, 0xC8, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0xC0, 0x20, 0x21, 0xC0,
    0x20, 0x18, 0xC0, 0x80, 0xC2
) "Tower-insect source"

# The Tower initializer clears its ten-step progress state, initializes its
# per-actor counter and a five-step global gate, then falls into 60C3. A hit
# awards 50 stored units (5,000 displayed points), emits command $33, and,
# when the child-count condition is met, advances $2D3F and enters 611C.
Assert-RomBytes $rom 0x0060B3 @(
    0x9C, 0x3F, 0x2D,
    0xA9, 0x03, 0x9D, 0x00, 0x3B,
    0xA9, 0x05, 0x8D, 0x4D, 0x2D,
    0x20, 0x18, 0xC0,
    0xBD, 0x00, 0x38, 0x10, 0x62,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xAD, 0x80, 0x35, 0x10, 0x58,
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA9, 0x33, 0x20, 0x87, 0xC0,
    0xAD, 0x2B, 0x2D, 0x3A, 0xD0, 0x13,
    0xEE, 0x3F, 0x2D,
    0xAD, 0x3F, 0x2D, 0xC9, 0x0A, 0xD0, 0x09,
    0x9C, 0x3F, 0x2D,
    0xA9, 0x01, 0x85, 0xCC, 0x85, 0xCD,
    0xAD, 0xFC, 0x2C, 0xF0, 0x14,
    0xCE, 0x4D, 0x2D, 0xF0, 0x28,
    0xDE, 0x00, 0x3B, 0xD0, 0x22,
    0xA9, 0x03, 0x9D, 0x00, 0x3B,
    0x20, 0x69, 0xC0, 0x80, 0x05,
    0xA9, 0x01, 0x8D, 0xFC, 0x2C,
    0xA9, 0x04, 0x20, 0x21, 0xC0,
    0x20, 0x18, 0xC0,
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x9D, 0x00, 0x38,
    0x4C, 0xC0, 0x60, 0x60
) "Tower initializer and steady update"
Assert-RomBytes $rom 0x00202A @(
    0x4C, 0x4F, 0xF5
) "Score entry"
Assert-RomBytes $rom 0x002087 @(
    0x4C, 0xFD, 0xEC
) "Audio-command entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 2646 -or
    $writes.Count -ne 538 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 2,646 Tower calls, 538 scoped writes, and 3,872 actor " +
        "identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne "60C3" -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne "2D" -or
        $call.output_x -ne "2D" -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Tower bounded-call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Tower changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

if (
    [int] $calls[0].frame -ne 873 -or
    [int] $calls[-1].frame -ne 3600 -or
    [int64] $calls[0].body_master_cycles -ne 27 -or
    [int] $calls[0].write_count -ne 0 -or
    [int64] (($calls.body_master_cycles | ForEach-Object { [int64] $_ }) |
        Measure-Object -Maximum).Maximum -ne 5703
) {
    throw "Tower frame, replay sample, or timing range changed."
}

$hitMarkerWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "60D4" -and
            $_.work_ram_offset -eq "002F" -and
            $_.value -eq "32"
        }
)
if ($hitMarkerWrites.Count -ne 1) {
    throw "Expected one routed Tower hit."
}
$hitSequence = [int] $hitMarkerWrites[0].call_sequence
$hitCall = $calls[$hitSequence - 1]
$hitWrites = @($writes | Where-Object { [int] $_.call_sequence -eq $hitSequence })
if (
    $hitSequence -ne 2405 -or
    [int] $hitCall.frame -ne 3353 -or
    [int64] $hitCall.body_master_cycles -ne 5703 -or
    [int] $hitCall.write_count -ne 132 -or
    $hitCall.return_physical_bank -ne "01" -or
    $hitCall.return_instruction_pc -ne "C5E9"
) {
    throw "Routed Tower hit identity changed."
}

Assert-Write $hitWrites "03" "60CA" "182D" "00" "Tower hit latch clear"
Assert-Write $hitWrites "03" "60D6" "0030" "00" "Tower score high byte"
Assert-Write $hitWrites "00" "F587" "0205" "4C" "Tower selected score low byte"
Assert-Write $hitWrites "00" "F58F" "020C" "04" "Tower selected score middle byte"
Assert-Write $hitWrites "00" "F597" "0213" "00" "Tower selected score high byte"
Assert-Write $hitWrites "00" "ECFD" "002A" "33" "Tower audio command"
Assert-Write $hitWrites "03" "60E6" "0D3F" "01" "Tower progress step"
Assert-Write $hitWrites "03" "6114" "0CFC" "01" "Tower transition flag"
Assert-Write $hitWrites "01" "C5FB" "1F2D" "1C" "Tower transition low pointer"
Assert-Write $hitWrites "01" "C600" "1FAD" "61" "Tower transition high pointer"

$interruptOnlyCalls = @(
    $calls |
        Where-Object {
            [int] $_.write_count -gt 0 -and
            [int] $_.sequence -ne $hitSequence
        }
)
if (
    $interruptOnlyCalls.Count -ne 30 -or
    @($calls | Where-Object { [int] $_.write_count -eq 0 }).Count -ne 2615
) {
    throw "Tower no-hit/interrupt call population changed."
}

# Guard the fixed source and Tower identities plus the Tower's short
# transition away from and back to 60C3.
$sourceLow = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -eq "3F2C" -and
            [int] $_.frame -ge 872 -and
            $_.value -ne "00"
        }
)
$sourceHigh = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -eq "3FAC" -and
            [int] $_.frame -ge 872 -and
            $_.value -ne "00"
        }
)
$towerType = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -eq "302D" -and
            $_.value -eq "83"
        }
)
$towerLow = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -eq "3F2D" -and
            [int] $_.frame -ge 872 -and
            $_.value -ne "00"
        }
)
$towerHigh = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -eq "3FAD" -and
            [int] $_.frame -ge 872 -and
            $_.value -ne "00"
        }
)
if (
    ($sourceLow.frame -join ",") -ne "872,873,1085" -or
    ($sourceLow.value -join ",") -ne "25,4A,5F" -or
    ($sourceHigh.value -join ",") -ne "5F,5F,5F" -or
    $towerType.Count -ne 1 -or
    [int] $towerType[0].frame -ne 872 -or
    ($towerLow.frame -join ",") -ne "872,3353,3358,3359" -or
    ($towerLow.value -join ",") -ne "C3,1C,1F,C3" -or
    ($towerHigh.frame -join ",") -ne "872,3353,3358,3359" -or
    ($towerHigh.value -join ",") -ne "60,61,61,60"
) {
    throw "Fixed Tower-family actor identity changed."
}

# The source's BSR spawner creates four type-$82 children. Their detailed
# two-hit scoring/free behavior is guarded by dc_rom_two_hit_target.
$childTypeWrites = @(
    $actorWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5F80" -and
            $_.value -eq "82"
        }
)
if (
    ($childTypeWrites.frame -join ",") -ne "941,989,1037,1085" -or
    ($childTypeWrites.logical_address -join ",") -ne
        "3064,3067,306A,306D"
) {
    throw "Tower-insect child allocation identity changed."
}

Write-Output "Tower family semantics verified."
Write-Output (
    "Fixed Tower: slot 2D at (144,538), initializer 03:60B3, " +
    "2646 steady updates."
)
Write-Output (
    "Routed Tower hit: frame 3353 awards 50, emits command 33, and " +
    "advances progress 2D3F to 1."
)
Write-Output (
    "Tower insects: slot 2C at (140,526) spawns four 03:6016 children; " +
    "its group-clear award is 200."
)
Write-Output (
    "First normalized no-hit sample: 03:60C3@1, frame 873, " +
    "27 cycles, 0 writes."
)
