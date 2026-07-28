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
    [string] $StatePath
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

    $expected = @(
        $Hex -split "\s+" |
            Where-Object { $_ -ne "" } |
            ForEach-Object { [Convert]::ToByte($_, 16) }
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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Slot $4D is the fixed Undead Head at Roman-numeral selector I.
Assert-RomHex $rom 0x0048FF @"
00 4D A0 61 98 00 41 00 25 82 82 00
"@ "Undead Head fixed layout record"

# The predecessor selects bonus stage 2 and command $32. Its post-trigger
# continuation marks the ball and actor, calls RNG, selects $07/$09, stores
# continuation $6279, and enters the shared response helper at $4D28.
Assert-RomHex $rom 0x00622E @"
AD 80 30 09 20 8D 80 30 A5 53 09 10 85 53 A9 02
85 55 A9 32 20 87 C0 20 18 C0 A9 06 8D 80 39 AD
80 3C 09 01 8D 80 3C AD 80 30 09 20 8D 80 30 20
81 C0 A0 07 29 01 F0 02 A0 09 98 9D 80 3C A9 79
9D 00 3E A9 62 9D 80 3E 4C 28 4D
"@ "Undead Head stage-2 trigger and post-trigger selector"

Assert-RomHex $rom 0x002081 "4C B3 E8" "Shared RNG trampoline"
Assert-RomHex $rom 0x0008B3 @"
18 AD 2A 22 69 3F 85 42 AD 2B 22 69 50 85 43 06
42 26 43 18 AD 2A 22 65 00 85 42 AD 2B 22 65 43
85 43 18 AD 2A 22 65 43 8D 2B 22 A5 42 8D 2A 22
AD 2B 22 60
"@ "Shared E8B3 RNG recurrence"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 152 -or
    $writes.Count -ne 1386 -or
    $states.Count -ne 7807
) {
    throw "Undead-Head selector RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 150) {
    throw "Undead-Head selector route shared-RNG population changed."
}
$expectedRngHistogram = @{
    "282,8" = 140
    "939,20" = 3
    "975,21" = 4
    "1029,22" = 2
    "1041,23" = 1
}
$actualRngHistogram = @{}
$expectedPairs = @(
    "E8B9:0042",
    "E8C0:0043",
    "E8C2:0042",
    "E8C4:0043",
    "E8CC:0042",
    "E8D3:0043",
    "E8DB:022B",
    "E8E0:022A"
)
foreach ($call in $rngCalls) {
    if (
        $call.physical_bank -ne "00" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "E8E6"
    ) {
        throw "RNG call $($call.sequence) changed its bounded identity."
    }

    $histogramKey = "{0},{1}" -f (
        [int64] $call.body_master_cycles
    ), [int] $call.write_count
    if (-not $actualRngHistogram.ContainsKey($histogramKey)) {
        $actualRngHistogram[$histogramKey] = 0
    }
    ++$actualRngHistogram[$histogramKey]

    $callWrites = @(
        $writes |
            Where-Object {
                [int] $_.call_sequence -eq [int] $call.sequence -and
                $expectedPairs -contains (
                    "$($_.instruction_logical_pc):$($_.work_ram_offset)"
                )
            }
    )
    $actualPairs = @(
        $callWrites |
            ForEach-Object {
                "$($_.instruction_logical_pc):$($_.work_ram_offset)"
            }
    )
    if (($actualPairs -join ",") -ne ($expectedPairs -join ",")) {
        throw "RNG call $($call.sequence) changed direct write order."
    }
}
if (
    $actualRngHistogram.Count -ne $expectedRngHistogram.Count -or
    @(
        $expectedRngHistogram.GetEnumerator() |
            Where-Object {
                -not $actualRngHistogram.ContainsKey($_.Key) -or
                $actualRngHistogram[$_.Key] -ne $_.Value
            }
    ).Count -ne 0
) {
    throw "Undead-Head selector route RNG timing histogram changed."
}

$targetRng = $calls[0]
$continuation = $calls[1]
$consumer = $calls[2]
if (
    [int] $targetRng.sequence -ne 1 -or
    [int] $targetRng.frame -ne 1221 -or
    $targetRng.logical_pc -ne "E8B3" -or
    $targetRng.caller_pc -ne "625D" -or
    $targetRng.input_x -ne "4D" -or
    $targetRng.output_a -ne "73" -or
    [int64] $targetRng.body_master_cycles -ne 282 -or
    [int] $targetRng.write_count -ne 8 -or
    [int] $continuation.sequence -ne 2 -or
    [int] $continuation.frame -ne 1221 -or
    $continuation.physical_bank -ne "03" -or
    $continuation.logical_pc -ne "6248" -or
    $continuation.input_x -ne "4D" -or
    $continuation.output_y -ne "09" -or
    $continuation.return_physical_bank -ne "01" -or
    $continuation.return_instruction_pc -ne "C5E9" -or
    [int64] $continuation.body_master_cycles -ne 903 -or
    [int] $continuation.write_count -ne 29 -or
    [int] $consumer.sequence -ne 3 -or
    [int] $consumer.frame -ne 1221 -or
    $consumer.physical_bank -ne "03" -or
    $consumer.logical_pc -ne "625D" -or
    $consumer.input_x -ne "4D" -or
    $consumer.input_a -ne "A0" -or
    $consumer.output_y -ne "09" -or
    $consumer.return_physical_bank -ne "01" -or
    $consumer.return_instruction_pc -ne "C5E9" -or
    [int64] $consumer.body_master_cycles -ne 810 -or
    [int] $consumer.write_count -ne 26
) {
    throw "Undead-Head selector RNG call contract changed."
}

$expectedStateWrites = @(
    "1220:00:F54B:30CD:82",
    "1221:03:6252:3C80:01",
    "1221:03:625A:3080:A0",
    "1221:00:E8DB:222B:73",
    "1221:00:E8E0:222A:88",
    "1221:03:6269:3CCD:09",
    "1221:03:626E:3E4D:79",
    "1221:03:6273:3ECD:62",
    "1221:01:C5FB:3F4D:2D",
    "1221:01:C600:3FCD:4D"
)
foreach ($expected in $expectedStateWrites) {
    $parts = $expected.Split(":")
    $matches = @(
        $states |
            Where-Object {
                $_.frame -eq $parts[0] -and
                $_.instruction_physical_bank -eq $parts[1] -and
                $_.instruction_logical_pc -eq $parts[2] -and
                $_.logical_address -eq $parts[3] -and
                $_.value -eq $parts[4]
            }
    )
    if ($matches.Count -ne 1) {
        throw "Undead-Head selector state write '$expected' changed."
    }
}

Write-Output "Undead-Head selector RNG semantics verified."
Write-Output (
    "Activation: fixed slot 4D enters post-trigger state 03:6248 at frame 1221."
)
Write-Output (
    "RNG consumer 03:625D: result 73 selects response value 09."
)
Write-Output (
    "Continuation: stores 6279 and enters the shared 4D2D response state."
)
Write-Output (
    "Normalized samples: 03:6248@1 is 903 cycles and 03:625D@1 is 810 cycles."
)
