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
Assert-RomBytes $rom 0x002087 @(0x4C, 0xFD, 0xEC) "Audio-command entry"
Assert-RomBytes $rom 0x00202A @(0x4C, 0x4F, 0xF5) "Score entry"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 226 -or
    $writes.Count -ne 8999 -or
    $actorWrites.Count -ne 3872
) {
    throw (
        "Expected 226 Devil-Shooter calls, 8,999 scoped writes, and " +
        "3,872 actor identity writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @("5116", "5159") -or
        $call.caller_pc -ne "C51D" -or
        $call.return_pc -ne "C520" -or
        $call.input_x -ne "0E" -or
        $call.output_x -ne "0E" -or
        $call.input_sp -ne $call.output_sp
    ) {
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
} "Devil-Shooter routine"
Assert-Histogram $calls "return_instruction_pc" @{
    "5123" = 124
    "C5E9" = 2
    "EBD6" = 96
    "5178" = 4
} "Devil-Shooter return"

$idleCalls = @($calls | Where-Object { $_.logical_pc -eq "5116" })
$chargeCalls = @($calls | Where-Object { $_.logical_pc -eq "5159" })
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

$catchCall = $calls[124]
$heldCalls = @($calls[125..220])
$releaseDelayCalls = @($calls[221..224])
$releaseCall = $calls[225]
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

$catchWrites = @($writes | Where-Object { [int] $_.call_sequence -eq 125 })
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
    ($audioWrites.call_sequence -join ",") -ne "125,126,127,128,129,226"
) {
    throw "Devil-Shooter audio-command timing changed."
}

$chargeCounters = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "5302" -and
            $_.work_ram_offset -eq "0D15"
        }
)
if (
    $chargeCounters.Count -ne 96 -or
    $chargeCounters[0].call_sequence -ne "126" -or
    $chargeCounters[0].value -ne "00" -or
    $chargeCounters[-1].call_sequence -ne "221" -or
    $chargeCounters[-1].value -ne "18"
) {
    throw "Devil-Shooter charge progression changed."
}

$releaseWrites = @($writes | Where-Object { [int] $_.call_sequence -eq 226 })
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
    "First normalized idle sample: 02:5116@1, frame 872, 27 cycles, " +
    "0 writes."
)
