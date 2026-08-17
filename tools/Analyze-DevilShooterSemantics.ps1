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

# Fixed layout slot $0E starts at (264,630). Its initializer deliberately
# moves the live actor to Y=671, establishes a Y=632 reference position, and
# installs the first idle/catch state. This is the manual's Devil Shooter.
Assert-RomBytes $rom 0x00460B @(
    0x00, 0x0E, 0xEE, 0x50, 0x08, 0x01,
    0x76, 0x02, 0x05, 0x00, 0x82, 0x00
) "Devil-Shooter layout record"
Assert-RomBytes $rom 0x0050EE @(
    0xA9, 0x05, 0x9D, 0x00, 0x3A,
    0xA9, 0x9F, 0x9D, 0x00, 0x33,
    0xA9, 0x02, 0x9D, 0x80, 0x33,
    0xA9, 0x78, 0x9D, 0x00, 0x3E,
    0xA9, 0x02, 0x9D, 0x80, 0x3E,
    0xA9, 0x04, 0x9D, 0x80, 0x39,
    0xA9, 0x27, 0x8D, 0x15, 0x2D,
    0x64, 0xBD, 0x20, 0x18, 0xC0
) "Devil-Shooter initializer"

# The idle state waits for the collision helper to set bit 7, then marks the
# shooter occupied and enters the catch path at 52B1.
Assert-RomBytes $rom 0x005116 @(
    0xBD, 0x00, 0x38, 0x10, 0x08,
    0xA9, 0x01, 0x8D, 0x80, 0x3C,
    0x4C, 0xB1, 0x52, 0x60
) "Devil-Shooter idle/catch dispatch"
Assert-RomBytes $rom 0x0052B1 @(
    0xA9, 0x25, 0x20, 0x87, 0xC0,
    0xA9, 0x0E, 0x8D, 0x80, 0x31,
    0xA9, 0x01, 0x8D, 0x00, 0x32,
    0x20, 0x18, 0xC0, 0x20, 0x3B, 0x53,
    0x38, 0xBD, 0x00, 0x33, 0xE9, 0x09, 0x8D, 0x00, 0x33,
    0xBD, 0x80, 0x33, 0xE9, 0x00, 0x8D, 0x80, 0x33,
    0xAD, 0x15, 0x2D, 0xD0, 0x06,
    0x9C, 0x80, 0x3C, 0x4C, 0x24, 0x51, 0x60
) "Devil-Shooter catch rule"

# State 5159 reads controller-one Button I at $2013. While held it advances
# the ball position/charge counter through 52E3; after release it derives the
# launch velocity, emits command $2B, and advances the state machine.
Assert-RomBytes $rom 0x005159 @(
    0xAD, 0x54, 0x22, 0xF0, 0x03, 0x4C, 0xDA, 0x51,
    0xAC, 0x51, 0x22, 0xB9, 0x13, 0x20, 0x29, 0x01,
    0xD0, 0x0E, 0xBD, 0x00, 0x38, 0x10, 0x08,
    0xAD, 0x15, 0x2D, 0xF0, 0x03, 0x4C, 0xDA, 0x51, 0x60,
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x9D, 0x00, 0x38, 0x64, 0xBD,
    0xAD, 0x15, 0x2D, 0xD0, 0x05, 0xA9, 0x3D, 0x20, 0x87, 0xC0
) "Devil-Shooter Button-I charge entry"
Assert-RomBytes $rom 0x0051DA @(
    0xAD, 0x2C, 0x22, 0xF0, 0x0F,
    0xAD, 0x15, 0x2D, 0x4A, 0x4A, 0x4A, 0x1A, 0x1A,
    0x29, 0x0F, 0x49, 0xFF, 0x1A, 0x80, 0x0C,
    0xAD, 0x15, 0x2D, 0x4A, 0x4A, 0x1A, 0x1A,
    0x29, 0x0F, 0x49, 0xFF, 0x1A,
    0x8D, 0x80, 0x35, 0x9C, 0x80, 0x39,
    0xA9, 0x2B, 0x20, 0x87, 0xC0,
    0x20, 0x18, 0xC0, 0x20, 0x3B, 0x53
) "Devil-Shooter release rule"
Assert-RomBytes $rom 0x0052E3 @(
    0x18, 0xBD, 0x80, 0x32, 0x69, 0x40, 0x9D, 0x80, 0x32,
    0xBD, 0x00, 0x33, 0x69, 0x00, 0x9D, 0x00, 0x33,
    0x62, 0x7D, 0x80, 0x33, 0x9D, 0x80, 0x33,
    0x38, 0xBD, 0x00, 0x33, 0xFD, 0x00, 0x3E,
    0x8D, 0x15, 0x2D, 0xC9, 0x27, 0x90, 0x15
) "Devil-Shooter charge integrator"
Assert-RomBytes $rom 0x00533B @(
    0x38, 0xBD, 0x80, 0x32, 0xE9, 0x00, 0x9D, 0x80, 0x32,
    0xBD, 0x00, 0x33, 0xE9, 0x02, 0x9D, 0x00, 0x33,
    0xBD, 0x80, 0x33, 0xE9, 0x00, 0x9D, 0x80, 0x33,
    0x38, 0xBD, 0x00, 0x33, 0xFD, 0x00, 0x3E,
    0x8D, 0x15, 0x2D, 0xBD, 0x80, 0x33, 0xFD, 0x80, 0x3E
) "Devil-Shooter charge retraction"
Assert-RomBytes $rom 0x00525F @(
    0xBD, 0x00, 0x38, 0x30, 0x12,
    0xA5, 0x53, 0x29, 0x02, 0xD0, 0x01, 0x60,
    0xBD, 0x00, 0x38, 0x29, 0x7F, 0x9D, 0x00, 0x38,
    0x4C, 0x11, 0x51
) "Devil-Shooter post-launch gate"
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"
Assert-RomBytes $rom 0x00202A @(0x4C, 0x4F, 0xF5) "Score entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 2679 -or
    $writes.Count -ne 11229 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 2,679 Devil-Shooter calls, 11,229 scoped writes, and " +
        "3,872 actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("5116", "5159", "525F", "52E3", "533B") -or
        $call.input_x -ne "0E" -or
        $call.output_x -ne "0E" -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Devil-Shooter bounded-call identity changed at sequence $($index + 1)."
    }
    if ($call.logical_pc -eq "52E3") {
        if (
            (($call.caller_pc -ne "5194" -or $call.return_pc -ne "5197") -and
             ($call.caller_pc -ne "5249" -or $call.return_pc -ne "524C")) -or
            $call.return_instruction_pc -ne "533A"
        ) {
            throw "Devil-Shooter charge-position identity changed at sequence $($index + 1)."
        }
    } elseif ($call.logical_pc -eq "533B") {
        if (
            (($call.caller_pc -ne "52C3" -or $call.return_pc -ne "52C6") -and
             ($call.caller_pc -ne "5208" -or $call.return_pc -ne "520B")) -or
            $call.return_instruction_pc -ne "5393"
        ) {
            throw "Devil-Shooter charge-retraction identity changed at sequence $($index + 1)."
        }
    } elseif ($call.logical_pc -eq "525F") {
        if (
            $call.caller_pc -ne "C51D" -or $call.return_pc -ne "C520" -or
            $call.return_instruction_pc -ne "526A"
        ) {
            throw "Devil-Shooter post-launch identity changed at sequence $($index + 1)."
        }
    } elseif ($call.caller_pc -ne "C51D" -or $call.return_pc -ne "C520") {
        throw "Devil-Shooter bounded-call identity changed at sequence $($index + 1)."
    }
    # Held-charge calls exit through the audio bank and intentionally leave
    # MPR4/MPR5 remapped. The other MPRs remain part of the call contract.
    foreach ($mpr in @(0, 1, 2, 3, 6, 7)) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Devil Shooter changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "5116" = 125
    "5159" = 101
    "525F" = 2169
    "52E3" = 252
    "533B" = 32
} "Devil-Shooter routine"
Assert-Histogram $calls "return_instruction_pc" @{
    "5123" = 124
    "C5E9" = 2
    "EBD6" = 96
    "5178" = 4
    "533A" = 252
    "5393" = 32
    "526A" = 2169
} "Devil-Shooter return"

$idleCalls = @($calls | Where-Object { $_.logical_pc -eq "5116" })
$chargeCalls = @($calls | Where-Object { $_.logical_pc -eq "5159" })
$chargePositionCalls = @($calls | Where-Object { $_.logical_pc -eq "52E3" })
$chargeRetractionCalls = @($calls | Where-Object { $_.logical_pc -eq "533B" })
$postLaunchCalls = @($calls | Where-Object { $_.logical_pc -eq "525F" })
if (
    [int] $idleCalls[0].frame -ne 872 -or
    [int] $idleCalls[-1].frame -ne 1064 -or
    [int] $chargeCalls[0].frame -ne 1085 -or
    [int] $chargeCalls[-1].frame -ne 1185 -or
    [int64] $calls[0].body_master_cycles -ne 27 -or
    [int] $calls[0].write_count -ne 0 -or
    (@($idleCalls | Where-Object {
        $_.body_master_cycles -eq "27" -and $_.write_count -eq "0"
    })).Count -ne 123
) {
    throw "Devil-Shooter frame, idle-path, or replay sample changed."
}

if (
    $postLaunchCalls.Count -ne 2169 -or
    [int] $postLaunchCalls[0].frame -ne 1421 -or
    [int] $postLaunchCalls[-1].frame -ne 3600 -or
    (@($postLaunchCalls | Where-Object {
        $_.return_instruction_pc -eq "526A" -and
        $_.body_master_cycles -eq "45" -and
        $_.write_count -eq "0" -and
        $_.output_a -eq "00"
    })).Count -ne 2132 -or
    ($postLaunchCalls | Measure-Object -Property write_count -Sum).Sum -ne 485
) {
    throw "Devil-Shooter post-launch steady path changed."
}

$catchCall = $idleCalls[-1]
$heldCalls = @($chargeCalls | Where-Object { $_.return_instruction_pc -eq "EBD6" })
$releaseDelayCalls = @($chargeCalls | Where-Object { $_.return_instruction_pc -eq "5178" })
$releaseCall = @($chargeCalls | Where-Object { [int] $_.frame -eq 1185 })[0]
if (
    [int] $catchCall.frame -ne 1064 -or
    $catchCall.logical_pc -ne "5116" -or
    $catchCall.return_instruction_pc -ne "C5E9" -or
    [int64] $catchCall.body_master_cycles -ne 3993 -or
    [int] $catchCall.write_count -ne 101 -or
    $heldCalls.Count -ne 96 -or
    $releaseDelayCalls.Count -ne 4 -or
    [int] $releaseCall.frame -ne 1185 -or
    $releaseCall.return_instruction_pc -ne "C5E9" -or
    [int64] $releaseCall.body_master_cycles -ne 4650 -or
    [int] $releaseCall.write_count -ne 107
) {
    throw "Devil-Shooter catch/charge/release boundaries changed."
}

for ($index = 0; $index -lt $heldCalls.Count; ++$index) {
    $call = $heldCalls[$index]
    if (
        [int] $call.frame -ne 1085 + $index -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "EBD6" -or
        $call.output_a -ne "FF"
    ) {
        throw "Devil-Shooter held-input path changed at sequence $($call.sequence)."
    }
}
for ($index = 0; $index -lt $releaseDelayCalls.Count; ++$index) {
    $call = $releaseDelayCalls[$index]
    if (
        [int] $call.frame -ne 1181 + $index -or
        $call.return_physical_bank -ne "02" -or
        $call.return_instruction_pc -ne "5178" -or
        [int64] $call.body_master_cycles -ne 96 -or
        [int] $call.write_count -ne 0
    ) {
        throw "Devil-Shooter released-input delay changed at sequence $($call.sequence)."
    }
}

$catchWrites = @($writes | Where-Object {
    [int] $_.call_sequence -eq [int] $catchCall.sequence
})
Assert-SingleWrite $catchWrites "02" "511D" "1C80" "01" "Shooter occupied latch"
Assert-SingleWrite $catchWrites "00" "ECFD" "002A" "25" "Catch audio command"
Assert-SingleWrite $catchWrites "02" "52B8" "1180" "0E" "Caught-ball X low byte"
Assert-SingleWrite $catchWrites "02" "52BD" "1200" "01" "Caught-ball X high byte"

$audioWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -eq "ECFD" -and
            $_.work_ram_offset -eq "002A"
        }
)
Assert-Histogram $audioWrites "value" @{
    "25" = 1
    "3D" = 4
    "2B" = 1
} "Devil-Shooter audio command"
if (
    ($audioWrites.call_sequence -join ",") -ne "125,147,149,151,153,342"
) {
    throw "Devil-Shooter audio-command timing changed."
}

$chargeCounters = @(
    $writes |
        Where-Object {
            [int] $_.call_sequence -in @($chargePositionCalls | ForEach-Object {
                [int] $_.sequence
            }) -and
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5302" -and
            $_.work_ram_offset -eq "0D15"
        }
)
if (
    $chargePositionCalls.Count -ne 252 -or
    [int] $chargePositionCalls[0].frame -ne 1085 -or
    [int] $chargePositionCalls[-1].frame -ne 1420 -or
    (@($chargePositionCalls | Where-Object {
        $_.write_count -eq "5" -and
        $_.body_master_cycles -in @("243", "261", "279", "297")
    })).Count -ne 231 -or
    $chargeCounters.Count -ne 252 -or
    $chargeCounters[0].call_sequence -ne "146" -or
    $chargeCounters[0].value -ne "00" -or
    $chargeCounters[-1].call_sequence -ne "510" -or
    $chargeCounters[-1].value -ne "27"
) {
    throw "Devil-Shooter charge progression changed."
}

$retractionCounters = @(
    $writes |
        Where-Object {
            [int] $_.call_sequence -in @($chargeRetractionCalls | ForEach-Object {
                [int] $_.sequence
            }) -and
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "535B" -and
            $_.work_ram_offset -eq "0D15"
        }
)
if (
    $chargeRetractionCalls.Count -ne 32 -or
    [int] $chargeRetractionCalls[0].frame -ne 1065 -or
    [int] $chargeRetractionCalls[-1].frame -ne 1197 -or
    (@($chargeRetractionCalls | Where-Object {
        $_.write_count -in @("4", "5") -and
        $_.body_master_cycles -in @("234", "273", "291", "309", "327")
    })).Count -ne 28 -or
    $retractionCounters.Count -ne 32 -or
    $retractionCounters[0].call_sequence -ne "126" -or
    $retractionCounters[0].value -ne "25" -or
    $retractionCounters[-1].call_sequence -ne "354" -or
    $retractionCounters[-1].value -ne "00"
) {
    throw "Devil-Shooter charge retraction changed."
}

$releaseWrites = @($writes | Where-Object {
    [int] $_.call_sequence -eq [int] $releaseCall.sequence
})
Assert-SingleWrite $releaseWrites "02" "51FA" "1580" "F8" "Launch velocity"
Assert-SingleWrite $releaseWrites "02" "51FD" "1980" "00" "Launch animation reset"
Assert-SingleWrite $releaseWrites "00" "ECFD" "002A" "2B" "Launch audio command"

$scoreWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -in @("F587", "F58F", "F597", "F5A5")
        }
)
if ($scoreWrites.Count -ne 0) {
    throw "Devil-Shooter service unexpectedly changed score."
}

$identityWrites = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("300E", "3F0E", "3F8E") -and
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
    "871:300E=00", "871:3F0E=00", "871:3F8E=00",
    "871:300E=82", "871:3F0E=16", "871:3F8E=51",
    "1064:3F0E=C3", "1064:3F8E=52",
    "1084:3F0E=59", "1084:3F8E=51",
    "1185:3F0E=08", "1185:3F8E=52",
    "1197:3F0E=23", "1197:3F8E=52",
    "1264:3F0E=49", "1264:3F8E=52",
    "1420:3F0E=5F", "1420:3F8E=52"
) -join ","
if ($identityTimeline -ne $expectedIdentityTimeline) {
    throw "Devil-Shooter fixed identity or state timeline changed."
}

Write-Output "Devil-Shooter semantics verified."
Write-Output (
    "Fixed Devil Shooter: slot 0E, initializer 02:50EE, runtime " +
    "position (264,671)."
)
Write-Output (
    "Catch: frame 1064 emits 25, positions the ball at X=010E, and " +
    "transitions to charge."
)
Write-Output (
    "Charge/release: Button-I hold frames 1085-1180; command 3D x4; " +
    "release frame 1185 uses velocity F8 and command 2B."
)
Write-Output (
    "Post-launch: 2,169 collision-free/session-idle checks return at 02:526A."
)
Write-Output (
    "First normalized idle sample: 02:5116@1, frame 872, 27 cycles, " +
    "0 writes."
)
