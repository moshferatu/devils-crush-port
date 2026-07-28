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

function Assert-RomHex {
    param(
        [byte[]] $Rom,
        [int] $Offset,
        [string] $Hex,
        [string] $Label
    )

    [byte[]] $expected = @(
        $Hex.Split(
            [char[]] @(" ", "`r", "`n", "`t"),
            [System.StringSplitOptions]::RemoveEmptyEntries
        ) | ForEach-Object { [Convert]::ToByte($_, 16) }
    )
    if ($Offset -lt 0 -or $Offset + $expected.Count -gt $Rom.Count) {
        throw "$Label is outside the ROM."
    }
    for ($index = 0; $index -lt $expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}: expected {1:X2}, " +
                "found {2:X2}."
            ) -f (
                $Offset + $index
            ), $expected[$index], $Rom[$Offset + $index]
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

function Assert-Write {
    param(
        [object[]] $Rows,
        [string] $Bank,
        [string] $Pc,
        [string] $Offset,
        [string] $Value,
        [int] $Count,
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
    if ($matches.Count -ne $Count) {
        throw "$Label changed: expected $Count writes, found $($matches.Count)."
    }
}

function Assert-Call {
    param(
        [object] $Call,
        [int] $Sequence,
        [int] $Frame,
        [string] $Pc,
        [string] $X,
        [string] $Return,
        [int64] $Cycles,
        [int] $Writes,
        [string] $Label
    )

    if (
        [int] $Call.sequence -ne $Sequence -or
        [int] $Call.frame -ne $Frame -or
        $Call.logical_pc -ne $Pc -or
        $Call.input_x -ne $X -or
        $Call.return_instruction_pc -ne $Return -or
        [int64] $Call.body_master_cycles -ne $Cycles -or
        [int] $Call.write_count -ne $Writes
    ) {
        throw "$Label changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# The three records match the manual's three Skeleton Insects and its
# 4,000/10,000/4,000-point schedule.
Assert-RomHex $rom 0x0047AF @"
00 31 A3 54 20 00 66 02 07 C3 82 00
02 32 D3 55 84 00 B6 02 38 E0 81 00
00 33 43 55 F0 00 81 02 19 8B 82 00
"@ "Skeleton-Insect layout records"

# Left slot $31 initializes the trap-door-safety latch, waits for $2D13,
# awards 40 stored units, and advances through two animation phases.
Assert-RomHex $rom 0x0054A3 @"
9C 13 2D BD 00 38 09 40 9D 00 38 BD 80 30 09 40 9D
80 30 20 18 C0 AD 13 2D D0 01 60 A9 28 85 2F 64 30
20 2A C0 A9 04 9D 00 3A 20 18 C0 DE 00 3A D0 17 A9
04 9D 00 3A BD 00 38 09 40 9D 00 38 FE 80 39 BD 80
39 C9 03 F0 01 60 BD 80 30 29 BF 9D 80 30 BD 00 38
29 7F 9D 00 38 20 18 C0 BD 00 38 10 0B A9 28 85 2F
64 30 20 2A C0 80 10 A5 53 89 02 D0 0A 29 08 D0 06
AD C7 2C D0 01 60 A9 04 9D 00 3A 20 18 C0 DE 00 3A
D0 15 A9 04 9D 00 3A BD 00 38 09 40 9D 00 38 DE 80
39 D0 03 4C A3 54 60
"@ "Left Skeleton-Insect states"

# Right slot $33 mirrors the latch-triggered award through $2D14, with its
# own appearance count and post-trigger state.
Assert-RomHex $rom 0x005543 @"
9C 14 2D BD 00 38 09 40 9D 00 38 20 18 C0 AD 14 2D
D0 01 60 A9 28 85 2F 64 30 20 2A C0 A9 04 9D 00 3A
20 18 C0 DE 00 3A D0 17 A9 04 9D 00 3A BD 00 38 09
40 9D 00 38 FE 80 39 BD 80 39 C9 02 F0 01 60 BD 00
38 29 7F 9D 00 38 20 18 C0 A0 00 B9 00 32 F0 13 9C
32 2D BD 00 38 10 11 A9 28 85 2F 64 30 20 2A C0 80
06 AD C7 2C D0 01 60 A9 04 9D 00 3A 20 18 C0 DE 00
3A D0 15 A9 04 9D 00 3A BD 00 38 09 40 9D 00 38 DE
80 39 D0 03 4C 43 55 60
"@ "Right Skeleton-Insect states"

# Center slot $32 performs a staged initializer, waits for Tower progress
# $2CFC, awards 100 stored units, and runs three post-trigger states.
Assert-RomHex $rom 0x0055D3 @"
20 18 C0 20 18 C0 A9 02 20 9F C0 BD 80 30 09 60 9D
80 30 9C FC 2C A9 02 20 9F C0 BD 00 38 29 7F 9D 00
38 20 18 C0 AD FC 2C D0 01 60 A9 02 20 A2 C0 A9 64
85 2F 64 30 20 2A C0 BD 80 30 29 BF 9D 80 30 A9 10
20 21 C0 BD 80 30 49 20 9D 80 30 20 18 C0 BD 80 30
29 DF 9D 80 30 20 18 C0 A5 53 29 02 D0 0C A5 53 29
08 D0 06 AD C7 2C D0 01 60 A9 10 20 21 C0 BD 80 30
49 20 9D 80 30 20 18 C0 BD 80 30 09 60 9D 80 30 80
82
"@ "Center Skeleton-Insect states"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 5805 -or
    $writes.Count -ne 1316 -or
    $actorWrites.Count -ne 3882
) {
    throw (
        "Expected 5,805 Skeleton-Insect calls, 1,316 scoped writes, and " +
        "3,882 selected state writes."
    )
}

$allowedPcs = @(
    "54A3", "54B9", "54D0", "5500",
    "5543", "5551", "5568", "5590",
    "55D3", "55D6", "55D9", "55F9",
    "561A", "5625", "5630"
)
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin $allowedPcs -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Skeleton-Insect call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Skeleton-Insect call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "54A3" = 1
    "54B9" = 1631
    "54D0" = 12
    "5500" = 80
    "5543" = 1
    "5551" = 336
    "5568" = 8
    "5590" = 1084
    "55D3" = 1
    "55D6" = 1
    "55D9" = 1
    "55F9" = 2403
    "561A" = 16
    "5625" = 1
    "5630" = 229
} "Skeleton-Insect routine"

$leftWait = @($calls | Where-Object { $_.logical_pc -eq "54B9" })
$rightWait = @($calls | Where-Object { $_.logical_pc -eq "5551" })
$centerWait = @($calls | Where-Object { $_.logical_pc -eq "55F9" })
Assert-Histogram $leftWait "return_instruction_pc" @{
    "54BE" = 1630
    "C5E9" = 1
} "Left Skeleton-Insect return"
Assert-Histogram $rightWait "return_instruction_pc" @{
    "5556" = 335
    "C5E9" = 1
} "Right Skeleton-Insect return"
Assert-Histogram $centerWait "return_instruction_pc" @{
    "55FE" = 2402
    "C5E9" = 1
} "Center Skeleton-Insect return"

Assert-Call (
    @($rightWait | Where-Object { $_.return_instruction_pc -eq "C5E9" })[0]
) 1926 2182 "5551" "33" "C5E9" 978 23 (
    "Right Skeleton-Insect trigger"
)
Assert-Call (
    @($centerWait | Where-Object { $_.return_instruction_pc -eq "C5E9" })[0]
) 5207 3353 "55F9" "32" "C5E9" 3744 94 (
    "Center Skeleton-Insect trigger"
)
Assert-Call (
    @($leftWait | Where-Object { $_.return_instruction_pc -eq "C5E9" })[0]
) 5493 3466 "54B9" "31" "C5E9" 978 23 (
    "Left Skeleton-Insect trigger"
)

$leftAnimation = @($calls | Where-Object { $_.logical_pc -eq "54D0" })
$rightAnimation = @($calls | Where-Object { $_.logical_pc -eq "5568" })
$centerTransition = @($calls | Where-Object { $_.logical_pc -eq "561A" })
if (
    $leftAnimation[0].frame -ne "3467" -or
    $leftAnimation[-1].frame -ne "3478" -or
    $leftAnimation[-1].return_instruction_pc -ne "C5E9" -or
    $rightAnimation[0].frame -ne "2183" -or
    $rightAnimation[-1].frame -ne "2190" -or
    $rightAnimation[-1].return_instruction_pc -ne "C5E9" -or
    $centerTransition[0].frame -ne "3353" -or
    $centerTransition[-1].frame -ne "3370" -or
    @($calls | Where-Object {
        $_.logical_pc -eq "5625" -and
        $_.frame -eq "3371" -and
        $_.return_instruction_pc -eq "C5E9"
    }).Count -ne 1
) {
    throw "Skeleton-Insect post-trigger transition chronology changed."
}

# The Front-Spider and Tower source rules own the three trigger latches.
$triggerWrites = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("2D13", "2D14", "2CFC") -and
            [int] $_.frame -ge 872
        }
)
$actualTriggers = @(
    $triggerWrites |
        ForEach-Object {
            "$($_.frame):$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):$($_.logical_address):" +
                "$($_.value)"
        }
)
$expectedTriggers = @(
    "872:02:54A3:2D13:00",
    "872:02:5543:2D14:00",
    "941:02:55E6:2CFC:00",
    "2182:03:64AF:2D14:01",
    "2564:03:64AF:2D14:01",
    "3353:03:6114:2CFC:01",
    "3466:03:64AF:2D13:01"
)
if (($actualTriggers -join ",") -ne ($expectedTriggers -join ",")) {
    throw "Skeleton-Insect trigger-latch chronology changed."
}

# Each insect awards exactly once on this route. The source mechanisms own
# audible feedback; these target scopes do not call the audio producer.
Assert-Write $writes "02" "54C1" "002F" "28" 1 (
    "Left Skeleton-Insect award"
)
Assert-Write $writes "02" "5559" "002F" "28" 1 (
    "Right Skeleton-Insect award"
)
Assert-Write $writes "02" "5606" "002F" "64" 1 (
    "Center Skeleton-Insect award"
)
foreach ($pc in @("F587", "F58F", "F597", "F5A5")) {
    $scoreWrites = @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq $pc
            }
    )
    if ($scoreWrites.Count -ne 3) {
        throw "Skeleton-Insect score write $pc changed."
    }
}
if (
    @(
        $writes |
            Where-Object {
                $_.instruction_logical_pc -eq "ECFD" -or
                (
                    $_.instruction_physical_bank -eq "02" -and
                    $_.instruction_logical_pc -in @("5507", "55A1")
                )
            }
    ).Count -ne 0
) {
    throw "Skeleton-Insect inactive score/audio path became active."
}

# Guard the complete live-dispatch progression for each fixed slot.
$pointerLowWrites = @{
    "31" = @("B9", "D0", "00")
    "32" = @("D6", "D9", "F9", "1A", "25", "30")
    "33" = @("51", "68", "90")
}
$pointerHighWrites = @{
    "31" = @("54", "54", "55")
    "32" = @("55", "55", "55", "56", "56", "56")
    "33" = @("55", "55", "55")
}
foreach ($slot in @("31", "32", "33")) {
    $highAddress = "3F{0:X2}" -f (
        [Convert]::ToInt32($slot, 16) + 0x80
    )
    $low = @(
        $actorWrites |
            Where-Object {
                $_.logical_address -eq "3F$slot" -and
                [int] $_.frame -ge 872 -and
                $_.instruction_logical_pc -in @("C5E1", "C5FB")
            }
    )
    $high = @(
        $actorWrites |
            Where-Object {
                $_.logical_address -eq $highAddress -and
                [int] $_.frame -ge 872 -and
                $_.instruction_logical_pc -in @("C5E6", "C600")
            }
    )
    if (
        ($low.value -join ",") -ne ($pointerLowWrites[$slot] -join ",") -or
        ($high.value -join ",") -ne ($pointerHighWrites[$slot] -join ",")
    ) {
        throw "Skeleton-Insect slot $slot dispatch timeline changed."
    }
}

if (
    $leftWait[0].frame -ne "873" -or
    $leftWait[0].body_master_cycles -ne "21" -or
    $leftWait[0].write_count -ne "0" -or
    $rightWait[0].frame -ne "873" -or
    $rightWait[0].body_master_cycles -ne "21" -or
    $rightWait[0].write_count -ne "0" -or
    $centerWait[0].frame -ne "942" -or
    $centerWait[0].body_master_cycles -ne "21" -or
    $centerWait[0].write_count -ne "0"
) {
    throw "Skeleton-Insect normalized idle samples changed."
}

Write-Output "Skeleton-Insect semantics verified."
Write-Output (
    "Three fixed targets: left slot 31, center slot 32, right slot 33."
)
Write-Output (
    "Front-Spider latches 2D13/2D14 trigger +40 stored units on slots " +
    "31/33; the second right-latch write does not re-award."
)
Write-Output (
    "Tower latch 2CFC triggers +100 stored units on center slot 32."
)
Write-Output (
    "The three target scopes add no audio command; source mechanisms own " +
    "their same-frame feedback."
)
Write-Output (
    "Normalized idle samples: 02:54B9@1 and 02:5551@1 at frame 873; " +
    "02:55F9@1 at frame 942."
)
