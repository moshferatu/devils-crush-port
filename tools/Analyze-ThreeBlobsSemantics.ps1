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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The three adjacent category-2 records are the guide's Three Blobs above the
# central face. Their parameters 0/1/2 select both global state bytes
# $2D39-$2D3B and the initial internal states.
Assert-RomBytes $rom 0x004983 @(
    0x02, 0x58, 0x4A, 0x66, 0x80, 0x00,
    0xFE, 0x00, 0x41, 0x86, 0x82, 0x00,
    0x02, 0x59, 0x4A, 0x66, 0xA0, 0x00,
    0xFE, 0x00, 0x41, 0x86, 0x82, 0x01,
    0x02, 0x5A, 0x4A, 0x66, 0xC0, 0x00,
    0xFE, 0x00, 0x41, 0x86, 0x82, 0x02
) "Three-Blobs layout records"
Assert-RomBytes $rom 0x00664A @(
    0x9D, 0x80, 0x3A, 0xA8, 0x99, 0x39, 0x2D,
    0xB9, 0xE6, 0x67, 0x9D, 0x00, 0x3D,
    0xA9, 0x13, 0x9D, 0x00, 0x38,
    0xBD, 0x00, 0x33, 0x9D, 0x00, 0x3E,
    0xBD, 0x80, 0x33, 0x9D, 0x80, 0x3E,
    0xA9, 0x20, 0x9D, 0x80, 0x36,
    0x9E, 0x00, 0x34,
    0xA9, 0x01, 0x9D, 0x80, 0x34,
    0x20, 0x18, 0xC0
) "Three-Blobs initializer"

# 66A3 and 66D3 are two movement phases. Both converge on 676B, which
# handles motion, scene-state suspension, and the common collision/color
# handler at 67B3.
Assert-RomBytes $rom 0x0066A3 @(
    0xDE, 0x00, 0x3A, 0xD0, 0x0D,
    0xA9, 0x04, 0x9D, 0x00, 0x3A,
    0xBD, 0x80, 0x39, 0x49, 0x01, 0x9D, 0x80, 0x39,
    0xDE, 0x00, 0x3B, 0xF0, 0x28,
    0xDE, 0x80, 0x3B, 0xD0, 0x1F,
    0x38, 0x62, 0xFD, 0x00, 0x34, 0x9D, 0x00, 0x34,
    0x62, 0xFD, 0x80, 0x34, 0x9D, 0x80, 0x34,
    0xA9, 0x0C, 0x20, 0x21, 0xC0, 0x20, 0x12, 0xC0,
    0x20, 0x6B, 0x67, 0x20, 0x18, 0xC0, 0x80, 0xB9,
    0x20, 0x6B, 0x67, 0x60
) "Three-Blobs movement phases"
Assert-RomBytes $rom 0x0067B3 @(
    0xBC, 0x80, 0x3A, 0xB9, 0x39, 0x2D, 0xA8,
    0xB9, 0xE6, 0x67, 0x9D, 0x00, 0x3D,
    0xBD, 0x00, 0x38, 0x10, 0x20,
    0x29, 0x7F, 0x9D, 0x00, 0x38,
    0xA9, 0x44, 0x20, 0x87, 0xC0,
    0xBC, 0x80, 0x3A, 0xB9, 0x39, 0x2D,
    0x1A, 0xC9, 0x06, 0x90, 0x01, 0x62,
    0x99, 0x39, 0x2D, 0xA8,
    0xB9, 0xE6, 0x67, 0x9D, 0x00, 0x3D, 0x60,
    0x8A, 0x82, 0x85, 0x8A, 0x8E, 0x85
) "Three-Blobs collision and appearance cycle"

# A separate family state checks whether all three internal values match.
# State 4 is the guide's all-red result: it awards 100 stored units and sets
# bit 7 of $53. Other equal-state branches apply their own score/bonus rule,
# then randomize three consecutive states modulo four.
Assert-RomBytes $rom 0x0065BC @(
    0x9C, 0xFD, 0x2C, 0x20, 0x18, 0xC0,
    0xAD, 0x39, 0x2D, 0xCD, 0x3A, 0x2D, 0xD0, 0x1F,
    0xCD, 0x3B, 0x2D, 0xD0, 0x1A
) "Three-Blobs equal-state test"
Assert-RomBytes $rom 0x0065EA @(
    0xAD, 0x54, 0x22, 0xF0, 0x01, 0x60,
    0xA9, 0x64, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA5, 0x53, 0x09, 0x80, 0x85, 0x53, 0x80, 0x1A
) "Three-Blobs all-red result"
Assert-RomBytes $rom 0x00661B @(
    0xA9, 0x01, 0x8D, 0x3C, 0x2D,
    0x20, 0x81, 0xC0, 0x29, 0x03, 0x8D, 0x39, 0x2D,
    0x1A, 0x29, 0x03, 0x8D, 0x3A, 0x2D,
    0x1A, 0x29, 0x03, 0x8D, 0x3B, 0x2D
) "Three-Blobs post-match state reset"
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"
Assert-RomBytes $rom 0x00202A @(0x4C, 0x4F, 0xF5) "Score entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 1827 -or
    $writes.Count -ne 33397 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 1,827 Three-Blobs calls, 33,397 scoped writes, and " +
        "3,872 actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("66A3", "66D3") -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -notin @("58", "59", "5A") -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Three-Blobs bounded-call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Three Blobs changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

$callKeys = @(
    $calls |
        ForEach-Object {
            [pscustomobject] @{ Key = "$($_.logical_pc):$($_.input_x)" }
        }
)
Assert-Histogram $callKeys "Key" @{
    "66A3:58" = 369
    "66A3:59" = 369
    "66A3:5A" = 369
    "66D3:58" = 240
    "66D3:59" = 240
    "66D3:5A" = 240
} "Three-Blobs routine/slot"
Assert-Histogram $calls "return_instruction_pc" @{
    "66E1" = 1041
    "C5E9" = 786
} "Three-Blobs return"

$phaseOne = @($calls | Where-Object { $_.logical_pc -eq "66A3" })
$phaseTwo = @($calls | Where-Object { $_.logical_pc -eq "66D3" })
if (
    [int] $phaseOne[0].frame -ne 1229 -or
    [int] $phaseOne[-1].frame -ne 3600 -or
    [int] $phaseTwo[0].frame -ne 1235 -or
    [int] $phaseTwo[-1].frame -ne 2061 -or
    [int64] $phaseOne[0].body_master_cycles -ne 282 -or
    [int] $phaseOne[0].write_count -ne 6 -or
    ($phaseOne | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Minimum).Minimum -ne 282 -or
    ($phaseOne | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Maximum).Maximum -ne 4686 -or
    ($phaseTwo | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Minimum).Minimum -ne 1008 -or
    ($phaseTwo | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Maximum).Maximum -ne 24168
) {
    throw "Three-Blobs frame, timing, or replay sample changed."
}

$stateWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -eq "67DB" -and
            $_.work_ram_offset -in @("0D39", "0D3A", "0D3B")
        }
)
$expectedSequences = @(
    80, 587, 588, 591, 607, 611, 613, 614,
    616, 617, 619, 620, 621, 624, 727, 770
)
$expectedFrames = @(
    1254, 1546, 1546, 1547, 1552, 1553, 1554, 1554,
    1555, 1555, 1556, 1556, 1557, 1558, 1592, 1609
)
$expectedSlots = @(
    "5A", "5A", "5A", "5A", "59", "5A", "59", "5A",
    "59", "5A", "59", "5A", "59", "5A", "59", "59"
)
$expectedStates = @(
    "03", "04", "04", "05", "02", "00", "03", "01",
    "04", "02", "05", "03", "00", "04", "01", "02"
)
if ($stateWrites.Count -ne 16) {
    throw "Expected 16 Three-Blobs collision state writes."
}
for ($index = 0; $index -lt $stateWrites.Count; ++$index) {
    $write = $stateWrites[$index]
    $call = $calls[[int] $write.call_sequence - 1]
    $expectedOffset = if ($call.input_x -eq "59") {
        "0D3A"
    } else {
        "0D3B"
    }
    if (
        [int] $write.call_sequence -ne $expectedSequences[$index] -or
        [int] $call.frame -ne $expectedFrames[$index] -or
        $call.input_x -ne $expectedSlots[$index] -or
        $write.value -ne $expectedStates[$index] -or
        $write.work_ram_offset -ne $expectedOffset
    ) {
        throw "Three-Blobs collision $($index + 1) changed."
    }
}

$audioWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "ECFD" -and
            $_.work_ram_offset -eq "002A" -and
            $_.value -eq "44"
        }
)
if (
    $audioWrites.Count -ne 16 -or
    ($audioWrites.call_sequence -join ",") -ne (
        $expectedSequences -join ","
    )
) {
    throw "Three-Blobs command-44 producer schedule changed."
}

$scoreWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -in @("F587", "F58F", "F597", "F5A5")
        }
)
if ($scoreWrites.Count -ne 0) {
    throw "Three-Blobs collisions unexpectedly changed score."
}

$initialIdentity = @(
    $actorWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.logical_address -in @(
                "3058", "3059", "305A",
                "3F58", "3F59", "3F5A",
                "3FD8", "3FD9", "3FDA"
            )
        }
)
$firstSteadyPointers = @(
    $actorWrites |
        Where-Object {
            [int] $_.frame -eq 1228 -and
            $_.logical_address -in @(
                "3F58", "3F59", "3F5A",
                "3FD8", "3FD9", "3FDA"
            )
        }
)
$alternatePointers = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("3F58", "3F59", "3F5A") -and
            $_.value -eq "0A"
        }
)
if (
    ($initialIdentity.value -join ",") -ne (
        "00,00,00,82,78,66,00,00,00,82,78,66,00,00,00,82,78,66"
    ) -or
    ($firstSteadyPointers.logical_address -join ",") -ne (
        "3F58,3FD8,3F59,3FD9,3F5A,3FDA"
    ) -or
    ($firstSteadyPointers.value -join ",") -ne "A3,66,A3,66,A3,66" -or
    ($alternatePointers.frame -join ",") -ne "1601,1639,1668,1971,1985,1993" -or
    ($alternatePointers.logical_address -join ",") -ne (
        "3F5A,3F59,3F58,3F5A,3F58,3F59"
    )
) {
    throw "Three-Blobs fixed identity or state timeline changed."
}

Write-Output "Three-Blobs semantics verified."
Write-Output (
    "Fixed Three Blobs: slots 58/59/5A at (128/160/192,254), " +
    "initial states 0/1/2."
)
Write-Output (
    "Collision cycle: 16 scoreless state advances, command 44 x16; " +
    "slot 58 is unhit and frame 1546 contains the coalesced duplicate."
)
Write-Output (
    "Appearance rule: six internal states select four animation IDs; " +
    "the all-state-4 result is ROM-bounded but inactive."
)
Write-Output (
    "First normalized sample: 03:66A3@1, frame 1229, 282 cycles, 6 writes."
)
