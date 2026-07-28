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
    [string] $TracePath,

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

function Get-StoredScore {
    param(
        [byte[]] $Bytes,
        [int] $Frame
    )

    return (
        (Get-FrameByte $Bytes $Frame 0x0205) +
        256 * (Get-FrameByte $Bytes $Frame 0x020C) +
        65536 * (Get-FrameByte $Bytes $Frame 0x0213)
    )
}

function Assert-GlyphStream {
    param(
        [object[]] $Trace,
        [int] $Frame,
        [string[]] $Expected,
        [string] $Label
    )

    $actual = @(
        $Trace |
            Where-Object {
                [int] $_.frame -eq $Frame -and
                $_.physical_bank -eq "09" -and
                $_.logical_pc -eq "72FD"
            } |
            ForEach-Object { $_.a }
    )
    if (($actual -join ",") -ne ($Expected -join ",")) {
        throw (
            "$Label changed at frame ${Frame}: expected " +
            "$($Expected -join ','), found $($actual -join ',')."
        )
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The normal-table ball handler integrates slot 0, applies the signed
# $02D0 drain threshold, and raises session-transition bit 1.
Assert-RomBytes $rom 0x002765 @(
    0x20, 0xF3, 0xC8,
    0x20, 0x09, 0xDD,
    0x20, 0x6E, 0xDD,
    0xB0, 0x03,
    0x4C, 0x95, 0xDC
) "C765 active-ball drain gate"
Assert-RomBytes $rom 0x002786 @(
    0xA5, 0x53, 0x09, 0x02, 0x85, 0x53,
    0xA5, 0x55, 0xF0, 0x01, 0x60
) "C786 normal-table drain request"

# The one-player drain coroutine decrements the live-ball count. Later it
# distinguishes no balls from a remaining-ball continuation and tests the
# shared transition latch before taking the ordinary C80A presentation.
Assert-RomBytes $rom 0x0027B0 @(
    0xAD, 0x2D, 0x22, 0xF0, 0x03, 0x4C, 0x42, 0xC8,
    0xAD, 0x54, 0x22, 0xD0, 0x3F,
    0xA0, 0x27, 0xCE, 0x52, 0x2D, 0xD0, 0x02, 0xA0, 0x47
) "C7B0 one-player ball decrement"
Assert-RomBytes $rom 0x0027E5 @(
    0xAD, 0x52, 0x2D, 0xF0, 0x0A,
    0xAD, 0x32, 0x2D, 0xF0, 0x1B,
    0x9C, 0x32, 0x2D, 0x80, 0x08,
    0xA9, 0xFF, 0x20, 0xEA, 0xC5, 0x20, 0xC9, 0xC5,
    0xA9, 0x01, 0x8D, 0x53, 0x2D, 0x20, 0xC9, 0xC5,
    0xAD, 0x53, 0x2D, 0xF0, 0x01, 0x60,
    0xA9, 0x10, 0x20, 0xEA, 0xC5, 0x20, 0xC9, 0xC5
) "C7E5 remaining-ball decision"
Assert-RomBytes $rom 0x002812 @(
    0xAD, 0x52, 0x2D, 0xF0, 0x21,
    0xA5, 0x53, 0x29, 0xFD, 0x85, 0x53, 0xDA,
    0xA9, 0x03, 0x85, 0x2F,
    0xA9, 0x02, 0x85, 0x30,
    0x20, 0x41, 0xE2, 0xFA,
    0x20, 0x74, 0xC5,
    0xAD, 0xC0, 0x2C, 0x09, 0x01, 0x8D, 0xC0, 0x2C,
    0x4C, 0x55, 0xC6
) "C812 remaining-ball selective reload"

# F55C owns the binary score and F5A9 saturates it at 9,999,999 stored
# units. The panel consumes dirty bit 0, emits two fixed zero glyphs, then
# converts the stored value to decimal. It renders $2D52 - 1 separately as
# the count of balls remaining after the active ball.
Assert-RomBytes $rom 0x00157E @(
    0xAC, 0x51, 0x22, 0x18,
    0xA5, 0x2F, 0x79, 0x05, 0x22, 0x99, 0x05, 0x22,
    0xA5, 0x30, 0x79, 0x0C, 0x22, 0x99, 0x0C, 0x22,
    0xB9, 0x13, 0x22, 0x69, 0x00, 0x99, 0x13, 0x22,
    0x20, 0xA9, 0xF5, 0x20, 0xD6, 0xF5,
    0xAD, 0xC0, 0x2C, 0x09, 0x01, 0x8D, 0xC0, 0x2C, 0x60
) "F55C selected score accumulation"
Assert-RomBytes $rom 0x0015A9 @(
    0xAC, 0x51, 0x22, 0x38,
    0xB9, 0x05, 0x22, 0xE9, 0x7F,
    0xB9, 0x0C, 0x22, 0xE9, 0x96,
    0xB9, 0x13, 0x22, 0xE9, 0x98,
    0x90, 0x17,
    0xA9, 0x7F, 0x99, 0x05, 0x22,
    0xA9, 0x96, 0x99, 0x0C, 0x22,
    0xA9, 0x98, 0x99, 0x13, 0x22,
    0xAD, 0xC0, 0x2C, 0x09, 0x80, 0x8D, 0xC0, 0x2C, 0x60
) "F5A9 score saturation"
Assert-RomBytes $rom 0x01325E @(
    0xAD, 0xC0, 0x2C, 0x89, 0x01, 0xD0, 0x01, 0x60,
    0x29, 0xFE, 0x8D, 0xC0, 0x2C
) "725E score-panel dirty gate"
Assert-RomBytes $rom 0x013283 @(
    0x62, 0x20, 0xFD, 0x72,
    0x62, 0x20, 0xFD, 0x72,
    0xA9, 0x01, 0x85, 0x31,
    0xB9, 0x05, 0x22, 0x85, 0x2E,
    0xB9, 0x0C, 0x22, 0x85, 0x2F,
    0xB9, 0x13, 0x22, 0x85, 0x30,
    0xA9, 0x0A, 0x85, 0x2D, 0x20, 0xD7, 0xE7,
    0xE6, 0x31, 0x20, 0xFD, 0x72,
    0xA5, 0x2E, 0x05, 0x2F, 0xD0, 0xF2,
    0x05, 0x30, 0xD0, 0xEE
) "7283 stored-score decimal conversion"
Assert-RomBytes $rom 0x0132B4 @(
    0x38, 0xA9, 0x08, 0xE5, 0x31, 0xF0, 0x0A,
    0x85, 0x31, 0x62, 0x20, 0xFD, 0x72, 0xC6, 0x31, 0xD0, 0xF8,
    0xA9, 0xBD, 0x85, 0x35, 0x64, 0x36,
    0xAD, 0x52, 0x2D, 0x3A, 0x10, 0x01, 0x62, 0x85, 0x30,
    0xA9, 0x0A, 0x85, 0x2F, 0x20, 0xDC, 0xE6,
    0xA5, 0x2F, 0x20, 0xFD, 0x72,
    0xA5, 0x30, 0x20, 0xFD, 0x72
) "72B4 score padding and remaining-ball conversion"

$recordingLines = Get-Content -LiteralPath $RecordingPath
if (
    "# work_ram_patch_count=2" -notin $recordingLines -or
    "# work_ram_patch_1=3000:3380:03" -notin $recordingLines -or
    "# work_ram_patch_2=3000:3300:00" -notin $recordingLines
) {
    throw "Session/scoring recording patch metadata changed."
}

$workRam = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $WorkRamPath)
)
if ($workRam.Count -ne 3600 * 8192) {
    throw "Expected 3,600 complete work-RAM frames."
}

Assert-FrameByte $workRam 2999 0x0053 0x00 "Pre-drain session state"
Assert-FrameByte $workRam 3000 0x0053 0x02 "Drain session request"
Assert-FrameByte $workRam 3015 0x0D52 0x03 "Pre-decrement ball count"
Assert-FrameByte $workRam 3016 0x0D52 0x02 "Post-decrement ball count"
Assert-FrameByte $workRam 3271 0x0D32 0x00 "Ordinary continuation latch"
Assert-FrameByte $workRam 3286 0x0053 0x02 "Pre-reload session state"
Assert-FrameByte $workRam 3287 0x0053 0x08 "Reloaded launcher session state"
Assert-FrameByte $workRam 3287 0x1300 0x8F "Reloaded ball Y low"
Assert-FrameByte $workRam 3287 0x1380 0x02 "Reloaded ball Y high"

for ($frame = 3000; $frame -le 3287; ++$frame) {
    if ((Get-FrameByte $workRam $frame 0x0D32) -ne 0) {
        throw "Alternate transition latch became active at frame $frame."
    }
}
for ($frame = 2962; $frame -le 3600; ++$frame) {
    if ((Get-StoredScore $workRam $frame) -ne 1020) {
        throw "Stored score changed after the final routed award at frame $frame."
    }
}

$trace = Import-TraceCsv $TracePath
if ($trace.Count -ne 402) {
    throw "Expected 402 ordered entries, found $($trace.Count)."
}
$eventEntries = @(
    $trace |
        Where-Object {
            $_.logical_pc -in @(
                "C786", "C7B0", "C7BF", "C7E5",
                "C7EA", "C80A", "C812", "4003"
            )
        } |
        ForEach-Object {
            "$($_.frame):$($_.physical_bank):$($_.logical_pc)"
        }
)
$expectedEventEntries = @(
    "3000:01:C786",
    "3016:01:C7B0",
    "3016:01:C7BF",
    "3271:01:C7E5",
    "3271:01:C7EA",
    "3271:01:C80A",
    "3287:01:C812",
    "3287:02:4003"
)
if (($eventEntries -join ",") -ne ($expectedEventEntries -join ",")) {
    throw "Drain-to-continuation entry order changed."
}

Assert-GlyphStream $trace 1409 @(
    "00", "00", "00", "01", "00", "00", "00", "00", "00", "02", "00"
) "First-award score and remaining-ball glyph stream"
Assert-GlyphStream $trace 2962 @(
    "00", "00", "00", "02", "00", "01", "00", "00", "00", "02", "00"
) "Pre-drain score and remaining-ball glyph stream"
Assert-GlyphStream $trace 3287 @(
    "00", "00", "00", "02", "00", "01", "00", "00", "00", "01", "00"
) "Post-drain score and remaining-ball glyph stream"

$calls = Import-TraceCsv $CallPath
if ($calls.Count -ne 402) {
    throw "Expected 402 bounded calls, found $($calls.Count)."
}
$scoreCalls = @($calls | Where-Object logical_pc -eq "F55C")
$panelCalls = @($calls | Where-Object logical_pc -eq "7266")
$glyphCalls = @($calls | Where-Object logical_pc -eq "72FD")
if (
    $scoreCalls.Count -ne 33 -or
    $panelCalls.Count -ne 29 -or
    $glyphCalls.Count -ne 332
) {
    throw "Score or panel call populations changed."
}
foreach ($call in $scoreCalls) {
    if (
        $call.physical_bank -ne "00" -or
        $call.caller_pc -ne "F554" -or
        $call.return_instruction_pc -ne "F5A8"
    ) {
        throw "Score call $($call.sequence) changed its bounded path."
    }
}

$expectedDecisionCalls = @{
    C786 = @(3000, 2877, 83)
    C7B0 = @(3016, 4806, 114)
    C7BF = @(3016, 4752, 114)
    C7E5 = @(3271, 366, 13)
    C7EA = @(3271, 345, 13)
    C80A = @(3271, 318, 13)
    C812 = @(3287, 105072, 2268)
}
foreach ($pc in $expectedDecisionCalls.Keys) {
    $matches = @($calls | Where-Object logical_pc -eq $pc)
    $expected = $expectedDecisionCalls[$pc]
    if (
        $matches.Count -ne 1 -or
        [int] $matches[0].frame -ne $expected[0] -or
        [int64] $matches[0].body_master_cycles -ne $expected[1] -or
        [int] $matches[0].write_count -ne $expected[2] -or
        $matches[0].caller_pc -ne "C51D" -or
        $matches[0].return_instruction_pc -ne "C5E9"
    ) {
        throw "Decision call $pc changed."
    }
}
$sceneCall = @($calls | Where-Object logical_pc -eq "4003")
if (
    $sceneCall.Count -ne 1 -or
    [int] $sceneCall[0].frame -ne 3287 -or
    [int64] $sceneCall[0].body_master_cycles -ne 98931 -or
    [int] $sceneCall[0].write_count -ne 2095 -or
    $sceneCall[0].return_instruction_pc -ne "40CE"
) {
    throw "Category-1 continuation call changed."
}

$writes = Import-TraceCsv $WritePath
if ($writes.Count -ne 27769) {
    throw "Expected 27,769 scoped writes, found $($writes.Count)."
}
$directStorePcs = @("F587", "F58F", "F597", "F5A5")
$expectedOffsets = @("0205", "020C", "0213", "0CC0")
$previousScore = 0
$awards = [System.Collections.Generic.HashSet[int]]::new()
foreach ($call in $scoreCalls) {
    $directWrites = @(
        $writes |
            Where-Object {
                $_.call_sequence -eq $call.sequence -and
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -in $directStorePcs
            }
    )
    if (
        $directWrites.Count -ne 4 -or
        ($directWrites.instruction_logical_pc -join ",") -ne
            ($directStorePcs -join ",") -or
        ($directWrites.work_ram_offset -join ",") -ne
            ($expectedOffsets -join ",")
    ) {
        throw "Score call $($call.sequence) changed direct write order."
    }

    $score =
        [Convert]::ToInt32($directWrites[0].value, 16) -bor
        ([Convert]::ToInt32($directWrites[1].value, 16) -shl 8) -bor
        ([Convert]::ToInt32($directWrites[2].value, 16) -shl 16)
    $award = $score - $previousScore
    if ($award -ne [Convert]::ToInt32($call.input_a, 16)) {
        throw "Score call $($call.sequence) changed its award."
    }
    if ($directWrites[3].value -ne "01") {
        throw "Score call $($call.sequence) did not set panel dirty bit 0."
    }
    [void] $awards.Add($award)
    $previousScore = $score
}
if (
    $previousScore -ne 1020 -or
    (@($awards | Sort-Object) -join ",") -ne "10,30,40,50,200"
) {
    throw "Routed score accumulation changed."
}

function Assert-SingleScopedWrite {
    param(
        [object[]] $Calls,
        [object[]] $Writes,
        [string] $CallPc,
        [string] $InstructionPc,
        [string] $Offset,
        [string] $Value,
        [string] $Label
    )

    $call = @($Calls | Where-Object logical_pc -eq $CallPc)
    $matches = @(
        $Writes |
            Where-Object {
                $_.call_sequence -eq $call[0].sequence -and
                $_.instruction_logical_pc -eq $InstructionPc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($call.Count -ne 1 -or $matches.Count -ne 1) {
        throw "$Label changed."
    }
}

Assert-SingleScopedWrite $calls $writes "C786" "C78A" "0053" "02" `
    "Drain request write"
Assert-SingleScopedWrite $calls $writes "C7BF" "C7BF" "0D52" "02" `
    "Ball decrement write"
Assert-SingleScopedWrite $calls $writes "C812" "C832" "0CC0" "01" `
    "Continuation panel-dirty write"
$finalPanelCall = @(
    $panelCalls | Where-Object { [int] $_.frame -eq 3287 }
)
if ($finalPanelCall.Count -ne 1) {
    throw "Expected one post-drain panel update."
}
$panelClear = @(
    $writes |
        Where-Object {
            $_.call_sequence -eq $finalPanelCall[0].sequence -and
            $_.instruction_physical_bank -eq "09" -and
            $_.instruction_logical_pc -eq "7268" -and
            $_.work_ram_offset -eq "0CC0" -and
            $_.value -eq "00"
        }
)
if ($panelClear.Count -ne 1) {
    throw "Post-drain panel dirty-bit consumption changed."
}

Write-Output "Session-and-scoring route verified."
Write-Output (
    "Drain: frame 3000 raises session bit 1; frame 3016 decrements " +
    "the live-ball count from 3 to 2."
)
Write-Output (
    "Continuation: the zero transition latch selects ordinary C80A at " +
    "frame 3271; C812 and 4003 reload the normal table at frame 3287."
)
Write-Output (
    "Score display: stored 1020 emits nine score digits for 102000 " +
    "visible points; remaining-ball digits change from 02 to 01."
)
Write-Output (
    "No multiplier, extra-ball grant, alternate transition, or score " +
    "mutation occurs on this ordinary remaining-ball route."
)
Write-Output (
    "Maximum display: saturation 9999999 and two fixed zero glyphs " +
    "produce 999999900 visible points."
)
