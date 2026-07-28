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

function Select-StateWrites {
    param(
        [object[]] $States,
        [string] $Address,
        [string] $Bank,
        [string] $Pc
    )

    return @(
        $States |
            Where-Object {
                $_.logical_address -eq $Address -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc
            }
    )
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

Assert-RomHex $rom 0x004863 @"
00 40 72 59 48 00 37 00 14 C2 82 00
00 41 72 59 E8 00 37 00 15 C2 82 00
"@ "Upper Wall-Pocket hand records"

# The post-close response state maps the RNG high byte to $B0-$EF, passes it
# to the common response helper, advances twice for the upper-pair region,
# and starts the reverse animation before returning to the delayed state.
Assert-RomHex $rom 0x0059F5 @"
20 81 C0 29 3F 18 69 B0 20 21 C0 AD 33 2D D0 03
20 18 C0 20 18 C0 A9 01 9D 00 3A BD 80 30 09 40
9D 80 30 20 18 C0 DE 00 3A D0 10 A9 04 9D 00 3A
BD 80 39 D0 03 4C 75 59 DE 80 39 60
"@ "Upper-hand post-close response and reverse pass"

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
    $calls.Count -ne 24 -or
    $writes.Count -ne 260 -or
    $states.Count -ne 4493
) {
    throw "Upper-hand response RNG artifact populations changed."
}

$rngCalls = @($calls | Where-Object { $_.logical_pc -eq "E8B3" })
if ($rngCalls.Count -ne 23) {
    throw "Upper-hand response route RNG population changed."
}
foreach ($call in $rngCalls) {
    $expectedCycles = switch ([int] $call.sequence) {
        9 { 975 }
        13 { 939 }
        14 { 1029 }
        default { 282 }
    }
    $expectedWriteCount = switch ([int] $call.sequence) {
        9 { 21 }
        13 { 20 }
        14 { 22 }
        default { 8 }
    }
    if (
        $call.physical_bank -ne "00" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "E8E6" -or
        [int64] $call.body_master_cycles -ne $expectedCycles -or
        [int] $call.write_count -ne $expectedWriteCount
    ) {
        throw "RNG call $($call.sequence) changed its bounded contract."
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

$firstRng = $calls[0]
$response = $calls[1]
if (
    [int] $firstRng.sequence -ne 1 -or
    [int] $firstRng.frame -ne 1221 -or
    $firstRng.logical_pc -ne "E8B3" -or
    $firstRng.caller_pc -ne "59F5" -or
    $firstRng.input_x -ne "40" -or
    $firstRng.output_a -ne "73" -or
    [int] $response.sequence -ne 2 -or
    [int] $response.frame -ne 1221 -or
    $response.physical_bank -ne "02" -or
    $response.logical_pc -ne "59F5" -or
    $response.caller_pc -ne "C51D" -or
    $response.input_x -ne "40" -or
    $response.output_a -ne "E3" -or
    $response.return_physical_bank -ne "01" -or
    $response.return_instruction_pc -ne "C5E9" -or
    [int64] $response.body_master_cycles -ne 1416 -or
    [int] $response.write_count -ne 37
) {
    throw "Upper-hand post-close response call contract changed."
}

# Result $73 masks to $33 and becomes response $E3. The upper-pair branch
# advances twice, installs state 02:5A00, and begins its E3-tick delay.
Assert-ScopeWrite $writes 2 "01" "C5EA" "1DC0" "E3" `
    "Upper-hand response delay"
Assert-ScopeWrite $writes 2 "01" "C5FB" "1F40" "00" `
    "Upper-hand response continuation low"
Assert-ScopeWrite $writes 2 "01" "C600" "1FC0" "5A" `
    "Upper-hand response continuation high"
Assert-ScopeWrite $writes 2 "01" "C5D4" "1DC0" "E2" `
    "Upper-hand first response countdown"

$newSeedHigh = @(
    $writes |
        Where-Object {
            [int] $_.call_sequence -eq 1 -and
            $_.instruction_logical_pc -eq "E8DB" -and
            $_.work_ram_offset -eq "022B"
        }
)
if ($newSeedHigh.Count -ne 1 -or $newSeedHigh[0].value -ne "73") {
    throw "Upper-hand response RNG result changed."
}

$activation = @(Select-StateWrites $states "30C0" "00" "F54B")
$responseStart = @(Select-StateWrites $states "3DC0" "01" "C5EA")
$responseCountdown = @(Select-StateWrites $states "3DC0" "01" "C5D4")
$nextTimerLow = @(Select-StateWrites $states "3B40" "02" "5986")
$nextTimerHigh = @(Select-StateWrites $states "3BC0" "02" "598B")
if (
    $activation.Count -ne 94 -or
    $activation[0].frame -ne "1220" -or
    $activation[0].value -ne "C2" -or
    $responseStart.Count -ne 1 -or
    $responseStart[0].frame -ne "1221" -or
    $responseStart[0].value -ne "E3" -or
    $responseCountdown.Count -ne 227 -or
    $responseCountdown[0].frame -ne "1221" -or
    $responseCountdown[0].value -ne "E2" -or
    $responseCountdown[-1].frame -ne "1447" -or
    $responseCountdown[-1].value -ne "00" -or
    $nextTimerLow.Count -ne 1 -or
    $nextTimerLow[0].frame -ne "1450" -or
    $nextTimerLow[0].value -ne "32" -or
    $nextTimerHigh.Count -ne 1 -or
    $nextTimerHigh[0].frame -ne "1450" -or
    $nextTimerHigh[0].value -ne "01"
) {
    throw "Upper-hand response or return-to-delay timeline changed."
}

Write-Output "Upper Wall-Pocket hand response RNG semantics verified."
Write-Output (
    "Activation: fixed slot 40 enters response state 02:59F5 at frame 1221."
)
Write-Output (
    "RNG consumer 02:59F5: result 73 maps to response E3."
)
Write-Output (
    "Response continuation: 02:5A00 counts E3 to zero through frame 1447."
)
Write-Output (
    "Cycle return: the reverse pass reaches 02:597D at frame 1450."
)
