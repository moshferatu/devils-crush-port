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

function Assert-ScopeWrite {
    param(
        [object[]] $Writes,
        [int] $Call,
        [string] $Bank,
        [string] $Pc,
        [string] $Offset,
        [string] $Value,
        [string] $Label
    )

    $selected = @(
        $Writes |
            Where-Object {
                [int] $_.call_sequence -eq $Call -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($selected.Count -ne 1) {
        throw "$Label changed."
    }
}

function Assert-StateWrite {
    param(
        [object[]] $States,
        [int] $Frame,
        [string] $Bank,
        [string] $Pc,
        [string] $Address,
        [string] $Value,
        [string] $Label
    )

    $selected = @(
        $States |
            Where-Object {
                [int] $_.frame -eq $Frame -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.logical_address -eq $Address -and
                $_.value -eq $Value
            }
    )
    if ($selected.Count -ne 1) {
        throw "$Label changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Equality is tested before a six-way outcome dispatch. State zero reaches
# the default 10-unit award at 65DE and then converges on randomized reset.
Assert-RomHex $rom 0x0065C2 @"
AD 39 2D CD 3A 2D D0 1F CD 3B 2D D0 1A 3A F0 2F
3A F0 3A 3A F0 06 3A F0 0F 3A F0 31 A9 0A 85 2F
64 30 20 2A C0 80 32 60
"@ "Three-Blobs equality and state-zero outcome"

Assert-RomHex $rom 0x00661B @"
A9 01 8D 3C 2D 20 81 C0 29 03 8D 39 2D 1A 29 03
8D 3A 2D 1A 29 03 8D 3B 2D A9 08 20 21 C0 20 18
C0 9C 3C 2D
"@ "Three-Blobs randomized consecutive-state reset"

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
    $calls.Count -ne 26 -or
    $writes.Count -ne 292 -or
    $states.Count -ne 3695
) {
    throw "Three-Blobs reset RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 23) {
    throw "Three-Blobs reset route RNG population changed."
}
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
        $call.return_instruction_pc -ne "E8E6" -or
        [int64] $call.body_master_cycles -ne 282 -or
        [int] $call.write_count -ne 8
    ) {
        throw "RNG call $($call.sequence) changed its bounded contract."
    }
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

$resetRng = $calls[0]
$outcome = $calls[1]
$reset = $calls[2]
$consumer = $calls[3]
if (
    [int] $resetRng.sequence -ne 1 -or
    [int] $resetRng.frame -ne 1200 -or
    $resetRng.logical_pc -ne "E8B3" -or
    $resetRng.caller_pc -ne "6620" -or
    $resetRng.input_x -ne "57" -or
    $resetRng.output_a -ne "5D" -or
    [int] $outcome.sequence -ne 2 -or
    $outcome.physical_bank -ne "03" -or
    $outcome.logical_pc -ne "65DE" -or
    $outcome.input_x -ne "57" -or
    $outcome.return_physical_bank -ne "01" -or
    $outcome.return_instruction_pc -ne "C5E9" -or
    [int64] $outcome.body_master_cycles -ne 2217 -or
    [int] $outcome.write_count -ne 55 -or
    [int] $reset.sequence -ne 3 -or
    $reset.logical_pc -ne "661B" -or
    [int64] $reset.body_master_cycles -ne 774 -or
    [int] $reset.write_count -ne 27 -or
    [int] $consumer.sequence -ne 4 -or
    $consumer.logical_pc -ne "6620" -or
    [int64] $consumer.body_master_cycles -ne 753 -or
    [int] $consumer.write_count -ne 26
) {
    throw "Three-Blobs equality outcome or reset call contract changed."
}

# State zero awards 10 stored units. RNG high byte $5D has low bits 1, so
# the reset writes 1 and then two consecutive modulo-four states, 2 and 3.
Assert-ScopeWrite $writes 2 "03" "65E0" "002F" "0A" `
    "Three-Blobs state-zero award input"
Assert-ScopeWrite $writes 2 "00" "F587" "0205" "0A" `
    "Three-Blobs score accumulator input"
Assert-ScopeWrite $writes 2 "03" "661D" "0D3C" "01" `
    "Three-Blobs reset latch"
Assert-ScopeWrite $writes 2 "00" "E8DB" "022B" "5D" `
    "Three-Blobs reset RNG output"
Assert-ScopeWrite $writes 2 "03" "6625" "0D39" "01" `
    "Three-Blobs first reset state"
Assert-ScopeWrite $writes 2 "03" "662B" "0D3A" "02" `
    "Three-Blobs second reset state"
Assert-ScopeWrite $writes 2 "03" "6631" "0D3B" "03" `
    "Three-Blobs third reset state"

Assert-StateWrite $states 1200 "00" "F5A5" "2CC0" "01" `
    "Three-Blobs score dirty flag"
Assert-StateWrite $states 1200 "03" "661D" "2D3C" "01" `
    "Three-Blobs visible reset latch"
Assert-StateWrite $states 1200 "03" "6625" "2D39" "01" `
    "Three-Blobs visible first state"
Assert-StateWrite $states 1200 "03" "662B" "2D3A" "02" `
    "Three-Blobs visible second state"
Assert-StateWrite $states 1200 "03" "6631" "2D3B" "03" `
    "Three-Blobs visible third state"
Assert-StateWrite $states 1208 "03" "663C" "2D3C" "00" `
    "Three-Blobs reset-latch clear"

Write-Output "Three-Blobs reset RNG semantics verified."
Write-Output (
    "Equality route: aligned state zero enters 03:65DE at frame 1200."
)
Write-Output (
    "Outcome: state zero submits 10 stored score units before reset."
)
Write-Output (
    "RNG consumer 03:6620: result 5D produces consecutive states 1/2/3."
)
Write-Output (
    "Normalized reset samples: 03:661B@1 is 774 cycles; " +
    "03:6620@1 is 753 cycles."
)
