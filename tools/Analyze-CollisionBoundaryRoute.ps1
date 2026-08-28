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
    [string] $WritePath,

    [Parameter(Mandatory = $true)]
    [string] $StateWritePath
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

function Get-SingleCall {
    param(
        [object[]] $Rows,
        [int] $Frame,
        [string] $Bank,
        [string] $Pc,
        [string] $InputX,
        [string] $Label
    )

    $matches = @(
        $Rows |
            Where-Object {
                [int] $_.frame -eq $Frame -and
                $_.physical_bank -eq $Bank -and
                $_.logical_pc -eq $Pc -and
                $_.input_x -eq $InputX
            }
    )
    if ($matches.Count -ne 1) {
        throw "$Label changed: expected one call, found $($matches.Count)."
    }
    return $matches[0]
}

function Get-CallsInside {
    param(
        [object[]] $Rows,
        [object] $Outer,
        [string] $Bank,
        [string] $Pc,
        [string] $InputX
    )

    $start = [int64] $Outer.entry_absolute_master_cycle
    $end = [int64] $Outer.return_absolute_master_cycle
    return @(
        $Rows |
            Where-Object {
                $_.physical_bank -eq $Bank -and
                $_.logical_pc -eq $Pc -and
                $_.input_x -eq $InputX -and
                [int64] $_.entry_absolute_master_cycle -ge $start -and
                [int64] $_.return_absolute_master_cycle -le $end
            }
    )
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

function Assert-ScoreBytes {
    param(
        [object[]] $Rows,
        [object] $Call,
        [string] $Low,
        [string] $Middle,
        [string] $High,
        [string] $Label
    )

    Assert-SingleWrite $Rows ([string] $Call.sequence) `
        "00" "F587" "0205" $Low "$Label low byte"
    Assert-SingleWrite $Rows ([string] $Call.sequence) `
        "00" "F58F" "020C" $Middle "$Label middle byte"
    Assert-SingleWrite $Rows ([string] $Call.sequence) `
        "00" "F597" "0213" $High "$Label high byte"
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# CBCF is entered only after the central ball/actor overlap test succeeds.
# It marks the target collision byte, records the target slot, masks the actor
# type, and tail-dispatches through the low/high tables at CBFA/CBFF.
Assert-RomBytes $rom 0x002BCF @(
    0xA4, 0x64, 0x98, 0xD5, 0xBD, 0xF0, 0x0F,
    0xA5, 0x78, 0x30, 0x0B,
    0xB9, 0x00, 0x38, 0x09, 0x80, 0x99, 0x00, 0x38,
    0x98, 0x95, 0xBD,
    0xB9, 0x00, 0x30, 0x29, 0x3F, 0xA8,
    0xC9, 0x02, 0x90, 0x02, 0x77, 0x78,
    0xB9, 0xFF, 0xCB, 0x48,
    0xB9, 0xFA, 0xCB, 0x48, 0x60
) "CBCF confirmed-contact dispatcher"
Assert-RomBytes $rom 0x002BFA @(
    0x24, 0x32, 0x32, 0x03, 0x04,
    0xD5, 0xCF, 0xCF, 0xCC, 0xCC
) "CBCF type-response vectors"

# Type $82 selects vector index 2 and therefore response CF33. That response
# derives reflected motion, calls the shared D1A2 ball-motion updater, marks
# the collision as handled, and continues through DC95.
Assert-RomBytes $rom 0x002F33 @(
    0xA5, 0x78, 0x10, 0x24,
    0xBD, 0x00, 0x38, 0x10, 0x03, 0x4C, 0xF8, 0xCD,
    0x20, 0x81, 0xD0, 0x20, 0x9E, 0xD0,
    0x38, 0x62, 0xE5, 0xBE, 0x85, 0xBE,
    0x62, 0xE5, 0xBF, 0x85, 0xBF,
    0x20, 0xD8, 0xCF, 0x20, 0xA2, 0xD1,
    0xB7, 0x77, 0x4C, 0x95, 0xDC
) "CF33 type-82 ball response"

# The negative type-response retry selects one of four signed whole-pixel
# vectors at 01:CEFB from the signs of the current ball velocity.
Assert-RomBytes $rom 0x002EFB @(
    0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xFF, 0xFF
) "CDF8 collision-retry offsets"

# 6966 interprets the actor's collision table and 7FB1 advances the
# Small-Soldier response state before the actor tests the collision bit.
Assert-RomBytes $rom 0x006966 @(
    0x0A, 0xA8,
    0xB9, 0x38, 0x6A, 0x85, 0x2F,
    0xB9, 0x39, 0x6A, 0x85, 0x30,
    0xBC, 0x80, 0x38, 0x10, 0x12,
    0x98, 0x29, 0x7F, 0x49, 0x01, 0x08, 0x09, 0x80,
    0x9D, 0x80, 0x38, 0x28, 0xF0, 0x02, 0x80, 0x0E,
    0x18, 0x60
) "6966 collision-table interpreter entry"
Assert-RomBytes $rom 0x007FB1 @(
    0xBD, 0x80, 0x3A, 0x29, 0xFE, 0x0A, 0x85, 0x26,
    0xBD, 0x80, 0x3C, 0x29, 0x03, 0x18, 0x65, 0x26,
    0x9D, 0x80, 0x39,
    0xDE, 0x00, 0x3A, 0xD0, 0x08,
    0xA9, 0x04, 0x9D, 0x00, 0x3A, 0xFE, 0x80, 0x3C,
    0x60
) "7FB1 actor collision response"

# The two-hit state consumes bit 7, submits 30 stored units and command $21,
# decrements its counter, then either holds subtype $0F or awards 50, updates
# the event-group count, and frees the ordinary actor.
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
) "7F57 two-hit collision consumer"
Assert-RomBytes $rom 0x002024 @(
    0x4C, 0xAB, 0xC5,
    0x4C, 0x74, 0xC5,
    0x4C, 0x4F, 0xF5
) "C024 allocation, C027 clear, and C02A score entries"
Assert-RomBytes $rom 0x002087 @(
    0x4C, 0xFD, 0xEC
) "C087 audio-command entry"

$recording = @(Import-TraceCsv $RecordingPath)
$calls = @(Import-TraceCsv $CallPath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
$workRam = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $WorkRamPath)
)
if (
    $recording.Count -ne 3600 -or
    $calls.Count -ne 97379 -or
    $stateWrites.Count -ne 8893 -or
    $workRam.Count -ne 3600 * 8192
) {
    throw "Collision-boundary route artifact dimensions changed."
}

$routineCounts = @{}
foreach ($group in @($calls | Group-Object physical_bank, logical_pc)) {
    $routineCounts[$group.Name.Replace(", ", ":")] = $group.Count
}
$expectedRoutineCounts = @{
    "01:CBCF" = 270
    "03:7F57" = 20867
    "03:6966" = 54149
    "03:7FB1" = 21987
    "00:F55C" = 40
    "00:ED18" = 66
}
foreach ($key in $expectedRoutineCounts.Keys) {
    if (
        -not $routineCounts.ContainsKey($key) -or
        $routineCounts[$key] -ne $expectedRoutineCounts[$key]
    ) {
        throw "$key route population changed."
    }
}

$contactFlags = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "CBDF" -and
            $_.logical_address -eq "3862" -and
            $_.value -eq "90"
        }
)
if (
    $contactFlags.Count -ne 2 -or
    ($contactFlags.frame -join ",") -ne "1479,1852"
) {
    throw "Slot-62 confirmed-contact timeline changed."
}

$contactCalls = @()
$actorCalls = @()
$collisionCalls = @()
$responseCalls = @()
$hitScoreCalls = @()
$audioCalls = @()
$expectedContactReturns = @("DCD7", "DD08")
$expectedActorReturnBanks = @("03", "01")
$expectedActorReturns = @("7F80", "C594")
$frames = @(1479, 1852)
for ($index = 0; $index -lt $frames.Count; ++$index) {
    $frame = $frames[$index]
    $flagCycle = [int64] $contactFlags[$index].absolute_master_cycle
    $matchingContacts = @(
        $calls |
            Where-Object {
                [int] $_.frame -eq $frame -and
                $_.physical_bank -eq "01" -and
                $_.logical_pc -eq "CBCF" -and
                [int64] $_.entry_absolute_master_cycle -lt $flagCycle -and
                [int64] $_.return_absolute_master_cycle -gt $flagCycle
            }
    )
    if ($matchingContacts.Count -ne 1) {
        throw "Frame-$frame contact flag left its owning CBCF call."
    }
    $contact = $matchingContacts[0]
    if (
        $contact.caller_pc -ne "CB65" -or
        $contact.return_pc -ne "CB68" -or
        $contact.input_x -ne "00" -or
        $contact.return_physical_bank -ne "01" -or
        $contact.return_instruction_pc -ne $expectedContactReturns[$index]
    ) {
        throw "Frame-$frame confirmed-contact call identity changed."
    }

    $actor = Get-SingleCall `
        $calls $frame "03" "7F57" "62" "Slot-62 collision consumer"
    if (
        $actor.caller_pc -ne "C51D" -or
        $actor.return_pc -ne "C520" -or
        $actor.return_physical_bank -ne $expectedActorReturnBanks[$index] -or
        $actor.return_instruction_pc -ne $expectedActorReturns[$index]
    ) {
        throw "Frame-$frame actor collision-call identity changed."
    }

    $collision = @(
        Get-CallsInside $calls $actor "03" "6966" "62"
    )
    $response = @(
        Get-CallsInside $calls $actor "03" "7FB1" "62"
    )
    $scores = @(
        Get-CallsInside $calls $actor "00" "F55C" "62"
    )
    $audio = @(
        Get-CallsInside $calls $actor "00" "ED18" "62"
    )
    if (
        $collision.Count -ne 1 -or
        $response.Count -ne 1 -or
        $audio.Count -ne 1 -or
        $scores.Count -ne ($index + 1)
    ) {
        throw "Frame-$frame nested collision effects changed."
    }
    if (
        $collision[0].caller_pc -ne "7F5A" -or
        $collision[0].return_instruction_pc -ne "6A37" -or
        $response[0].caller_pc -ne "7F5D" -or
        $response[0].return_instruction_pc -ne "7FD1" -or
        $audio[0].caller_pc -ne "7F75" -or
        $audio[0].return_instruction_pc -ne "ED85" -or
        $scores[0].input_a -ne "1E" -or
        $scores[0].return_instruction_pc -ne "F5A8"
    ) {
        throw "Frame-$frame nested collision-call identity changed."
    }
    if (
        [int64] $contact.return_absolute_master_cycle -ge
            [int64] $actor.entry_absolute_master_cycle -or
        [int64] $actor.entry_absolute_master_cycle -ge
            [int64] $collision[0].entry_absolute_master_cycle -or
        [int64] $collision[0].return_absolute_master_cycle -ge
            [int64] $response[0].entry_absolute_master_cycle -or
        [int64] $response[0].return_absolute_master_cycle -ge
            [int64] $scores[0].entry_absolute_master_cycle -or
        [int64] $scores[0].return_absolute_master_cycle -ge
            [int64] $audio[0].entry_absolute_master_cycle
    ) {
        throw "Frame-$frame contact/effect ordering changed."
    }
    if ($index -eq 1) {
        if (
            $scores[1].input_a -ne "32" -or
            [int64] $audio[0].return_absolute_master_cycle -ge
                [int64] $scores[1].entry_absolute_master_cycle
        ) {
            throw "Second-hit completion award changed."
        }
    }

    $contactCalls += $contact
    $actorCalls += $actor
    $collisionCalls += $collision[0]
    $responseCalls += $response[0]
    $hitScoreCalls += $scores[0]
    $audioCalls += $audio[0]
    if ($index -eq 1) {
        $completionScoreCall = $scores[1]
    }
}

Assert-FrameByte $workRam 1478 0x1C62 0x02 "Pre-hit actor counter"
Assert-FrameByte $workRam 1478 0x0205 0x0A "Pre-hit stored score"
Assert-FrameByte $workRam 1479 0x1862 0x10 "First-hit collision clear"
Assert-FrameByte $workRam 1479 0x1C62 0x01 "First-hit actor counter"
Assert-FrameByte $workRam 1479 0x1062 0x82 "First-hit actor type"
Assert-FrameByte $workRam 1479 0x0205 0x28 "First-hit stored score"
Assert-FrameByte $workRam 1851 0x1C62 0x01 "Pre-completion actor counter"
Assert-FrameByte $workRam 1851 0x0D16 0x05 "Pre-completion group count"
Assert-FrameByte $workRam 1851 0x0205 0x46 "Pre-completion stored score"
Assert-FrameByte $workRam 1852 0x1062 0x00 "Completed actor type clear"
Assert-FrameByte $workRam 1852 0x1862 0x00 "Completed collision-state clear"
Assert-FrameByte $workRam 1852 0x1C62 0x00 "Completed hit-counter clear"
Assert-FrameByte $workRam 1852 0x1F62 0x00 "Completed dispatch-low clear"
Assert-FrameByte $workRam 1852 0x1FE2 0x00 "Completed dispatch-high clear"
Assert-FrameByte $workRam 1852 0x0D16 0x04 "Completed group count"
Assert-FrameByte $workRam 1852 0x0205 0x96 "Completed stored score"

$selectedSequences = @{}
foreach ($call in @(
    $contactCalls + $actorCalls + $collisionCalls + $responseCalls +
    $hitScoreCalls + $audioCalls + @($completionScoreCall)
)) {
    $selectedSequences[[string] $call.sequence] = $true
}
$writes = @(Import-SelectedWrites $WritePath $selectedSequences)

$expectedContactMotion = @(
    @("BD", "00", "06", "02"),
    @("9B", "02", "09", "07")
)
for ($index = 0; $index -lt 2; ++$index) {
    $sequence = [string] $contactCalls[$index].sequence
    Assert-SingleWrite `
        $writes $sequence "01" "CBDF" "1862" "90" "Contact flag"
    Assert-SingleWrite `
        $writes $sequence "01" "D1FB" "1500" `
        $expectedContactMotion[$index][0] "Contact horizontal motion"
    Assert-SingleWrite `
        $writes $sequence "01" "D200" "1580" `
        $expectedContactMotion[$index][1] "Contact vertical motion"
    Assert-SingleWrite `
        $writes $sequence "01" "D21C" "1500" `
        $expectedContactMotion[$index][2] "Response horizontal motion"
    Assert-SingleWrite `
        $writes $sequence "01" "D224" "1580" `
        $expectedContactMotion[$index][3] "Response vertical motion"

    $actorSequence = [string] $actorCalls[$index].sequence
    Assert-SingleWrite `
        $writes $actorSequence "03" "7F67" "1862" "10" `
        "Actor collision-bit consumption"
    Assert-SingleWrite `
        $writes $actorSequence "03" "7F6C" "002F" "1E" `
        "Actor hit-award parameter"
    Assert-SingleWrite `
        $writes $actorSequence "03" "7F78" "1C62" `
        @("01", "00")[$index] "Actor hit-counter result"

    Assert-SingleWrite `
        $writes ([string] $audioCalls[$index].sequence) `
        "45" "4BCC" "00D4" "21" "Actor audio command"
}

Assert-ScoreBytes `
    $writes $hitScoreCalls[0] "28" "00" "00" "First-hit score"
Assert-ScoreBytes `
    $writes $hitScoreCalls[1] "64" "00" "00" "Second-hit score"
Assert-ScoreBytes `
    $writes $completionScoreCall "96" "00" "00" "Completion score"

$completionSequence = [string] $actorCalls[1].sequence
Assert-SingleWrite `
    $writes $completionSequence "03" "7F97" "002F" "32" `
    "Completion-award parameter"
Assert-SingleWrite `
    $writes $completionSequence "03" "7FAB" "0D16" "04" `
    "Completion group-count update"

$clearWrites = @(
    $writes |
        Where-Object {
            $_.call_sequence -eq $completionSequence -and
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C581" -and
            ([Convert]::ToInt32($_.work_ram_offset, 16) -band 0x7F) -eq
                0x62 -and
            $_.value -eq "00"
        }
)
if ($clearWrites.Count -ne 32) {
    throw "Standard completion stopped clearing all 32 slot-62 arrays."
}

Write-Output "Normal-table collision boundary verified."
Write-Output (
    "Contact: 01:CBCF marks slot 62 at frames 1479 and 1852 and " +
    "completes the type-82 ball response."
)
Write-Output (
    "Dispatch: each contact precedes 03:6966 and 03:7FB1 in its " +
    "03:7F57 actor call."
)
Write-Output (
    "Effects: each hit adds 30 stored units and emits command 21; the " +
    "second hit adds 50."
)
Write-Output (
    "Event: slot 62 reaches counter zero, `$2D16 becomes 4, and all 32 " +
    "actor arrays are cleared."
)
