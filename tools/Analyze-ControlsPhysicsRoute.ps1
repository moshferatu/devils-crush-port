# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $RecordingPath,

    [Parameter(Mandatory = $true)]
    [string] $WorkRamPath,

    [Parameter(Mandatory = $true)]
    [string] $CallPath,

    [Parameter(Mandatory = $true)]
    [string] $WritePath
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

function Assert-SingleCall {
    param(
        [object[]] $Rows,
        [int] $Frame,
        [string] $Routine,
        [string] $InputX,
        [string] $ReturnInstruction,
        [string] $Label
    )

    $parts = $Routine.Split(":")
    $matches = @(
        $Rows |
            Where-Object {
                [int] $_.frame -eq $Frame -and
                $_.physical_bank -eq $parts[0] -and
                $_.logical_pc -eq $parts[1] -and
                $_.input_x -eq $InputX -and
                $_.return_instruction_pc -eq $ReturnInstruction
            }
    )
    if ($matches.Count -ne 1) {
        throw "$Label changed: expected one call, found $($matches.Count)."
    }
    return $matches[0]
}

function Get-FrameByte {
    param(
        [byte[]] $Bytes,
        [int] $Frame,
        [int] $Offset
    )

    return $Bytes[(($Frame - 1) * 8192) + $Offset]
}

function Assert-FrameByte {
    param(
        [byte[]] $Bytes,
        [int] $Frame,
        [int] $Offset,
        [int] $Expected,
        [string] $Label
    )

    $actual = Get-FrameByte $Bytes $Frame $Offset
    if ($actual -ne $Expected) {
        throw (
            "$Label changed at frame $Frame, offset 0x{0:X4}: " +
            "expected {1:X2}, found {2:X2}."
        ) -f $Offset, $Expected, $actual
    }
}

function Import-SelectedWrites {
    param(
        [string] $Path,
        [hashtable] $Sequences
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Write trace does not exist: $Path"
    }
    $selected = [System.Collections.Generic.List[object]]::new()
    foreach ($line in [System.IO.File]::ReadLines(
        (Resolve-Path -LiteralPath $Path)
    )) {
        if ($line.StartsWith("#") -or $line.StartsWith("call_sequence,")) {
            continue
        }
        $firstComma = $line.IndexOf(",")
        if ($firstComma -le 0) {
            throw "Malformed routine-write trace row."
        }
        $sequence = $line.Substring(0, $firstComma)
        if (-not $Sequences.ContainsKey($sequence)) {
            continue
        }
        $fields = $line.Split(",")
        if ($fields.Count -ne 8) {
            throw "Malformed selected routine-write trace row."
        }
        $selected.Add([pscustomobject] @{
            call_sequence = $fields[0]
            write_sequence = $fields[1]
            master_cycle = $fields[2]
            absolute_master_cycle = $fields[3]
            instruction_physical_bank = $fields[4]
            instruction_logical_pc = $fields[5]
            work_ram_offset = $fields[6]
            value = $fields[7]
        })
    }
    return @($selected)
}

function Assert-SingleWrite {
    param(
        [object[]] $Rows,
        [string] $Sequence,
        [string] $Bank,
        [string] $Pc,
        [string] $Offset,
        [string] $Value,
        [string] $Label
    )

    $matches = @(
        $Rows |
            Where-Object {
                $_.call_sequence -eq $Sequence -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($matches.Count -ne 1) {
        throw "$Label changed: expected one write, found $($matches.Count)."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The central actor scheduler copies the active player's held-button byte
# into the flipper input mask and suppresses it while the table gate is set.
Assert-RomBytes $rom 0x002539 @(
    0xAE, 0x51, 0x22, 0xB4, 0x13, 0x8C, 0xEE, 0x2C,
    0xAD, 0x35, 0x2D, 0xF0, 0x03, 0x9C, 0xEE, 0x2C
) "Held input to flipper mask"

# D466 intersects the global input mask with the per-piece mask at $3B80,X.
# A hit marks the piece active and enters its response state.
Assert-RomBytes $rom 0x003466 @(
    0xAD, 0xEE, 0x2C, 0x3D, 0x80, 0x3B, 0xD0, 0x01, 0x60,
    0xA9, 0x01, 0x9D, 0x00, 0x3E, 0x9D, 0x00, 0x3A,
    0xE4, 0xBD, 0xD0, 0x10
) "D466 flipper input gate"

# C3CB first applies session and tilt gates, then tests Button II. Its
# position helper adds the signed nudge vector directly to slot-0 motion.
Assert-RomBytes $rom 0x0023CB @(
    0xA5, 0x53, 0x29, 0x02, 0xD0, 0x0B,
    0xAD, 0xC7, 0x2C, 0xD0, 0x06,
    0xA5, 0x53, 0x29, 0x28, 0xF0, 0x01, 0x60,
    0xCE, 0xC6, 0x2C, 0xD0, 0x08,
    0xA9, 0x78, 0x8D, 0xC6, 0x2C, 0x9C, 0xC5, 0x2C
) "C3CB session and tilt gates"
Assert-RomBytes $rom 0x0023F0 @(
    0xAC, 0x51, 0x22, 0xB9, 0x18, 0x20, 0x29, 0x02,
    0xD0, 0x01, 0x60
) "C3CB Button-II edge test"
Assert-RomBytes $rom 0x00245F @(
    0x29, 0x01, 0xD0, 0x21,
    0xAD, 0xC2, 0x2C, 0x85, 0x2F,
    0xAD, 0xC3, 0x2C, 0x85, 0x30,
    0x0A, 0x66, 0x30, 0x66, 0x2F, 0x82, 0x18,
    0xBD, 0x00, 0x35, 0x65, 0x2F, 0x9D, 0x00, 0x35,
    0xBD, 0x80, 0x35, 0x65, 0x30, 0x9D, 0x80, 0x35,
    0x60
) "C457 nudge-to-motion helper"

# The Devil Shooter reads held Button I, advances its charge path, and writes
# the final signed launch velocity into slot-0 motion.
Assert-RomBytes $rom 0x005159 @(
    0xAD, 0x54, 0x22, 0xF0, 0x03, 0x4C, 0xDA, 0x51,
    0xAC, 0x51, 0x22, 0xB9, 0x13, 0x20, 0x29, 0x01,
    0xD0, 0x0E
) "Devil-Shooter Button-I gate"
Assert-RomBytes $rom 0x0051F7 @(
    0x49, 0xFF, 0x1A, 0x8D, 0x80, 0x35,
    0x9C, 0x80, 0x39
) "Devil-Shooter launch velocity"

# F1B4 consumes the two signed 8.8 motion components and integrates the
# corresponding signed 16.8 slot position.
Assert-RomBytes $rom 0x0011FB @(
    0x64, 0x2B, 0xB5, 0xBA, 0x10, 0x02, 0xC6, 0x2B,
    0x18, 0xBD, 0x00, 0x31, 0x75, 0xB9, 0x9D, 0x00, 0x31,
    0xBD, 0x80, 0x31, 0x75, 0xBA, 0x9D, 0x80, 0x31,
    0xBD, 0x00, 0x32, 0x65, 0x2B, 0x9D, 0x00, 0x32
) "F1B4 slot-motion integration"

$recording = @(Import-TraceCsv $RecordingPath)
$calls = @(Import-TraceCsv $CallPath)
$workRam = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $WorkRamPath)
)
if (
    $recording.Count -ne 3600 -or
    $calls.Count -ne 16328 -or
    $workRam.Count -ne 3600 * 8192
) {
    throw "Controls/physics route artifact dimensions changed."
}

$expectedInputs = @{
    1000 = "0001"
    1180 = "0001"
    1181 = "0000"
    1265 = "0001"
    1266 = "0001"
    1280 = "0080"
    1281 = "0080"
    1450 = "0002"
    1451 = "0002"
    2250 = "0002"
    2251 = "0002"
    3050 = "0082"
    3051 = "0082"
}
foreach ($frame in $expectedInputs.Keys) {
    if ($recording[$frame - 1].input_p1 -ne $expectedInputs[$frame]) {
        throw "Scripted input changed at frame $frame."
    }
}

Assert-FrameByte $workRam 1000 0x0013 0x01 "Button-I held state"
Assert-FrameByte $workRam 1180 0x0013 0x01 "Final Button-I held state"
Assert-FrameByte $workRam 1181 0x0013 0x00 "Button-I release state"
Assert-FrameByte $workRam 1180 0x0D15 0x18 "Devil-Shooter charge"
Assert-FrameByte $workRam 1185 0x1580 0xF8 "Released launch velocity"
Assert-FrameByte $workRam 1265 0x0CEE 0x01 "Right-flipper input mask"
Assert-FrameByte $workRam 1265 0x1E09 0x01 "Right-flipper response state"
Assert-FrameByte $workRam 1265 0x1F09 0x8E "Right-flipper dispatch low"
Assert-FrameByte $workRam 1265 0x1F89 0xD4 "Right-flipper dispatch high"
Assert-FrameByte $workRam 1280 0x0CEE 0x80 "Left-flipper input mask"
Assert-FrameByte $workRam 1280 0x1E08 0x01 "Left-flipper response state"
Assert-FrameByte $workRam 1280 0x1F08 0x8E "Left-flipper dispatch low"
Assert-FrameByte $workRam 3050 0x0CEE 0x82 "Combined flipper/nudge input mask"
Assert-FrameByte $workRam 3050 0x1E04 0x01 "Combined left control slot 04"
Assert-FrameByte $workRam 3050 0x1E06 0x01 "Combined left control slot 06"

for ($frame = 1; $frame -le 3600; ++$frame) {
    if ((Get-FrameByte $workRam $frame 0x0CC7) -ne 0) {
        throw "The deterministic route unexpectedly entered tilt lock."
    }
}

$routineCounts = @{}
foreach ($group in @($calls | Group-Object physical_bank, logical_pc)) {
    $routineCounts[$group.Name.Replace(", ", ":")] = $group.Count
}
$expectedRoutineCounts = @{
    "02:5159" = 101
    "01:D466" = 7470
    "01:C3CB" = 2649
    "00:F1B4" = 3568
    "01:DD09" = 2540
}
foreach ($key in $expectedRoutineCounts.Keys) {
    if (
        -not $routineCounts.ContainsKey($key) -or
        $routineCounts[$key] -ne $expectedRoutineCounts[$key]
    ) {
        throw "$key route population changed."
    }
}

$launcherCalls = @($calls | Where-Object { $_.logical_pc -eq "5159" })
if (
    [int] $launcherCalls[0].frame -ne 1085 -or
    [int] $launcherCalls[-1].frame -ne 1185 -or
    (@($launcherCalls[0..95] | Where-Object {
        $_.return_instruction_pc -ne "EBD6"
    })).Count -ne 0 -or
    (@($launcherCalls[96..99] | Where-Object {
        $_.return_instruction_pc -ne "5178" -or
        $_.write_count -ne "0"
    })).Count -ne 0 -or
    $launcherCalls[-1].return_instruction_pc -ne "C5E9"
) {
    throw "Devil-Shooter held/release timeline changed."
}

$rightFlipper = Assert-SingleCall `
    $calls 1265 "01:D466" "09" "C5E9" "Right-flipper response"
$leftFlipper = Assert-SingleCall `
    $calls 1280 "01:D466" "08" "C5E9" "Left-flipper response"
$combinedLeft04 = Assert-SingleCall `
    $calls 3050 "01:D466" "04" "C5E9" "Combined left slot 04"
$combinedLeft06 = Assert-SingleCall `
    $calls 3050 "01:D466" "06" "C5E9" "Combined left slot 06"

$nudgeStarts = @()
$nudgeEnds = @()
foreach ($frame in @(1450, 2250, 3050)) {
    $inputX = if ($frame -eq 3050) { "67" } else { "6D" }
    $nudgeStarts += Assert-SingleCall `
        $calls $frame "01:C3CB" $inputX "C43E" "Nudge start"
    $nudgeEnds += Assert-SingleCall `
        $calls ($frame + 1) "01:C3CB" $inputX "C456" "Nudge completion"
}

$postEventPhysics = @()
foreach ($frame in @(1186, 1452, 2252, 3052)) {
    $matches = @(
        $calls |
            Where-Object {
                [int] $_.frame -eq $frame -and
                $_.physical_bank -eq "00" -and
                $_.logical_pc -eq "F1B4" -and
                $_.input_x -eq "00"
            }
    )
    if ($matches.Count -lt 1) {
        throw "Mapped slot-0 integration is missing at frame $frame."
    }
    $postEventPhysics += $matches[0]
}
if (
    [int] $launcherCalls[-1].sequence -ge
        [int] $postEventPhysics[0].sequence -or
    [int] $nudgeEnds[0].sequence -ge
        [int] $postEventPhysics[1].sequence -or
    [int] $nudgeEnds[1].sequence -ge
        [int] $postEventPhysics[2].sequence -or
    [int] $nudgeEnds[2].sequence -ge
        [int] $postEventPhysics[3].sequence
) {
    throw "Control events no longer precede their mapped physics calls."
}

$selectedSequences = @{}
foreach ($call in @(
    $launcherCalls +
    @($rightFlipper, $leftFlipper, $combinedLeft04, $combinedLeft06) +
    $nudgeStarts + $nudgeEnds + $postEventPhysics
)) {
    $selectedSequences[[string] $call.sequence] = $true
}
$writes = @(Import-SelectedWrites $WritePath $selectedSequences)

$chargeWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5302" -and
            $_.work_ram_offset -eq "0D15"
        }
)
if (
    $chargeWrites.Count -ne 96 -or
    $chargeWrites[0].value -ne "00" -or
    $chargeWrites[-1].value -ne "18"
) {
    throw "Devil-Shooter charge writes changed."
}
Assert-SingleWrite `
    $writes ([string] $launcherCalls[-1].sequence) `
    "02" "51FA" "1580" "F8" "Launcher-to-motion write"

foreach ($item in @(
    @($rightFlipper, "1E09"),
    @($leftFlipper, "1E08"),
    @($combinedLeft04, "1E04"),
    @($combinedLeft06, "1E06")
)) {
    Assert-SingleWrite `
        $writes ([string] $item[0].sequence) `
        "01" "D471" $item[1] "01" "Flipper response-state write"
}

$expectedNudgeMotion = @(
    @("11", "FE", "9E", "FD"),
    @("6B", "01", "FD", "00"),
    @("C3", "00", "54", "00")
)
for ($index = 0; $index -lt 3; ++$index) {
    Assert-SingleWrite `
        $writes ([string] $nudgeStarts[$index].sequence) `
        "01" "C479" "1500" $expectedNudgeMotion[$index][0] `
        "Nudge horizontal motion"
    Assert-SingleWrite `
        $writes ([string] $nudgeStarts[$index].sequence) `
        "01" "C481" "1580" $expectedNudgeMotion[$index][1] `
        "Nudge vertical motion"
    Assert-SingleWrite `
        $writes ([string] $nudgeEnds[$index].sequence) `
        "01" "C479" "1500" $expectedNudgeMotion[$index][2] `
        "Nudge follow-up horizontal motion"
    Assert-SingleWrite `
        $writes ([string] $nudgeEnds[$index].sequence) `
        "01" "C481" "1580" $expectedNudgeMotion[$index][3] `
        "Nudge follow-up vertical motion"
}

foreach ($call in $postEventPhysics) {
    $positionWrites = @(
        $writes |
            Where-Object {
                $_.call_sequence -eq [string] $call.sequence -and
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -in @(
                    "F1E8", "F1F0", "F1F8", "F209", "F211", "F219"
                )
            }
    )
    if ($positionWrites.Count -ne 6) {
        throw "F1B4 position integration changed after frame $($call.frame)."
    }
}

Write-Output "Controls-to-physics route verified."
Write-Output (
    "Launcher: Button I charges through frame 1180; frame 1185 writes " +
    "slot-0 velocity F8; frame 1186 integrates it."
)
Write-Output (
    "Flippers: right slot 09 responds at frame 1265; left slot 08 at " +
    "1280; combined left slots 04/06 at 3050."
)
Write-Output (
    "Nudge: Button II produces three C3CB motion pairs at frames " +
    "1450/1451, 2250/2251, and 3050/3051."
)
Write-Output "Tilt lock: `$2CC7 remains zero through all 3600 frames."
