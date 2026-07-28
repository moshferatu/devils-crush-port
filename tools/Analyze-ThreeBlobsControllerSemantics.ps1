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

function Assert-FrameFinalValue {
    param(
        [object[]] $Rows,
        [int] $Frame,
        [string] $Address,
        [string] $Expected
    )

    $matches = @(
        $Rows |
            Where-Object {
                [int] $_.frame -eq $Frame -and
                $_.logical_address -eq $Address
            }
    )
    if ($matches.Count -eq 0 -or $matches[-1].value -ne $Expected) {
        throw (
            "Frame $Frame final value at `$$Address changed: expected " +
            "$Expected."
        )
    }
}

function Assert-StateWrite {
    param(
        [object[]] $Rows,
        [int] $Frame,
        [string] $Bank,
        [string] $Pc,
        [string] $Address,
        [string] $Expected
    )

    $matches = @(
        $Rows |
            Where-Object {
                [int] $_.frame -eq $Frame -and
                $_.instruction_physical_bank -eq $Bank -and
                $_.instruction_logical_pc -eq $Pc -and
                $_.logical_address -eq $Address -and
                $_.value -eq $Expected
            }
    )
    if ($matches.Count -ne 1) {
        throw (
            "Expected one frame-$Frame write ${Bank}:${Pc} to " +
            "`$$Address=$Expected."
        )
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Slot $57 is the fixed controller immediately before the Three Blobs. It
# installs 03:65C2 and has no independently rendered appearance.
Assert-RomBytes $rom 0x004977 @(
    0x02, 0x57, 0xBC, 0x65, 0xC0, 0x00,
    0xFE, 0x00, 0x41, 0xE0, 0x82, 0x02
) "Three-Blobs controller layout record"

# The steady controller first requires all three family state bytes to be
# equal. The routed mismatch exits at 65E9 before any outcome-side write.
Assert-RomBytes $rom 0x0065BC @(
    0x9C, 0xFD, 0x2C, 0x20, 0x18, 0xC0,
    0xAD, 0x39, 0x2D, 0xCD, 0x3A, 0x2D, 0xD0, 0x1F,
    0xCD, 0x3B, 0x2D, 0xD0, 0x1A
) "Three-Blobs controller initializer and equality gate"
Assert-RomBytes $rom 0x0065CF @(
    0x3A, 0xF0, 0x2F, 0x3A, 0xF0, 0x3A,
    0x3A, 0xF0, 0x06, 0x3A, 0xF0, 0x0F,
    0x3A, 0xF0, 0x31, 0xA9, 0x0A, 0x85, 0x2F,
    0x64, 0x30, 0x20, 0x2A, 0xC0, 0x80, 0x32, 0x60
) "Three-Blobs equal-state outcome dispatch"

# State 4 is the guide's all-red outcome. It is conditional on $2254, awards
# 100 stored score units, and sets bit 7 of $53.
Assert-RomBytes $rom 0x0065EA @(
    0xAD, 0x54, 0x22, 0xF0, 0x01, 0x60,
    0xA9, 0x64, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0xA5, 0x53, 0x09, 0x80, 0x85, 0x53, 0x80, 0x1A
) "Three-Blobs all-red outcome"
Assert-RomBytes $rom 0x006601 @(
    0xA9, 0x32, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x66, 0xC0, 0x80, 0x0C,
    0xA9, 0x0A, 0x85, 0x2F, 0x64, 0x30, 0x20, 0x2A, 0xC0,
    0x20, 0x69, 0xC0
) "Three-Blobs alternate equal-state outcomes"
Assert-RomBytes $rom 0x00661B @(
    0xA9, 0x01, 0x8D, 0x3C, 0x2D,
    0x20, 0x81, 0xC0, 0x29, 0x03, 0x8D, 0x39, 0x2D,
    0x1A, 0x29, 0x03, 0x8D, 0x3A, 0x2D,
    0x1A, 0x29, 0x03, 0x8D, 0x3B, 0x2D
) "Three-Blobs post-outcome state reset"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 2651 -or
    $writes.Count -ne 673 -or
    $states.Count -ne 9366
) {
    throw (
        "Expected 2,651 controller calls, 673 scoped writes, and " +
        "9,366 selected state writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin @("65BC", "65C2") -or
        $call.input_x -ne "57" -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Three-Blobs controller identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Controller changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

$initializer = $calls[0]
if (
    $initializer.logical_pc -ne "65BC" -or
    [int] $initializer.frame -ne 872 -or
    $initializer.caller_pc -ne "4096" -or
    $initializer.return_pc -ne "4099" -or
    $initializer.return_physical_bank -ne "01" -or
    $initializer.return_instruction_pc -ne "C5E9" -or
    [int64] $initializer.body_master_cycles -ne 207 -or
    [int] $initializer.write_count -ne 8
) {
    throw "Three-Blobs controller initializer changed."
}

$steady = @($calls | Where-Object { $_.logical_pc -eq "65C2" })
if (
    $steady.Count -ne 2650 -or
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600 -or
    @(
        $steady | Where-Object {
            $_.caller_pc -ne "C51D" -or
            $_.return_pc -ne "C520" -or
            $_.return_physical_bank -ne "03" -or
            $_.return_instruction_pc -ne "65E9"
        }
    ).Count -ne 0
) {
    throw "Three-Blobs controller steady-call boundary changed."
}

$firstMismatch = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 42 -and
            [int] $_.write_count -eq 0
        }
)
$thirdMismatch = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 63 -and
            [int] $_.write_count -eq 0
        }
)
$interruptCalls = @(
    $steady |
        Where-Object { [int] $_.write_count -gt 0 }
)
if (
    $firstMismatch.Count -ne 2565 -or
    $thirdMismatch.Count -ne 35 -or
    $interruptCalls.Count -ne 50 -or
    ($interruptCalls |
        Measure-Object -Property body_master_cycles -Minimum
    ).Minimum -ne 564 -or
    ($interruptCalls |
        Measure-Object -Property body_master_cycles -Maximum
    ).Maximum -ne 1458 -or
    ($interruptCalls |
        Measure-Object -Property write_count -Minimum
    ).Minimum -ne 10 -or
    ($interruptCalls |
        Measure-Object -Property write_count -Maximum
    ).Maximum -ne 24
) {
    throw "Three-Blobs controller mismatch or interrupt timing changed."
}

# Controller-owned writes in this route are limited to initialization and
# interrupt stack activity at the equality comparisons. None of the selected
# outcome state, score, or bonus fields is written by the steady controller.
$controllerWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            [Convert]::ToInt32($_.instruction_logical_pc, 16) -ge 0x65BC -and
            [Convert]::ToInt32($_.instruction_logical_pc, 16) -le 0x6640
        }
)
$unexpectedControllerWrites = @(
    $controllerWrites |
        Where-Object {
            $_.instruction_logical_pc -notin @(
                "65BC", "65BF", "65C2", "65C5", "65C8"
            )
        }
)
if (
    $unexpectedControllerWrites.Count -ne 0 -or
    @(
        $controllerWrites |
            Where-Object {
                $_.work_ram_offset -notin @(
                    "0CFD", "01F2", "01F3", "01F9", "01FA", "01FB"
                )
            }
    ).Count -ne 0
) {
    throw "Three-Blobs controller unexpectedly entered an outcome path."
}

$controllerStateWrites = @(
    $states |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            [Convert]::ToInt32($_.instruction_logical_pc, 16) -ge 0x65BC -and
            [Convert]::ToInt32($_.instruction_logical_pc, 16) -le 0x6640
        }
)
if (
    $controllerStateWrites.Count -ne 1 -or
    $controllerStateWrites[0].instruction_logical_pc -ne "65BC" -or
    $controllerStateWrites[0].logical_address -ne "2CFD" -or
    $controllerStateWrites[0].value -ne "00"
) {
    throw "Three-Blobs selected controller-state writes changed."
}

Assert-StateWrite $states 872 "02" "4131" "3057" "82"
Assert-StateWrite $states 872 "02" "412B" "30D7" "E0"
Assert-StateWrite $states 872 "02" "4125" "3957" "41"
Assert-StateWrite $states 872 "01" "C5E1" "3F57" "C2"
Assert-StateWrite $states 872 "01" "C5E6" "3FD7" "65"
Assert-StateWrite $states 872 "03" "65BC" "2CFD" "00"
Assert-FrameFinalValue $states 872 "2D39" "00"
Assert-FrameFinalValue $states 872 "2D3A" "01"
Assert-FrameFinalValue $states 872 "2D3B" "02"
Assert-FrameFinalValue $states 872 "3D58" "8A"
Assert-FrameFinalValue $states 872 "3D59" "82"
Assert-FrameFinalValue $states 872 "3D5A" "85"

$stateCounts = @{}
foreach ($address in @("2D39", "2D3A", "2D3B", "2D3C")) {
    $stateCounts[$address] = @(
        $states | Where-Object { $_.logical_address -eq $address }
    ).Count
}
if (
    $stateCounts["2D39"] -ne 2 -or
    $stateCounts["2D3A"] -ne 9 -or
    $stateCounts["2D3B"] -ne 10 -or
    $stateCounts["2D3C"] -ne 1
) {
    throw "Three-Blobs selected color-state census changed."
}

if (
    [int64] $steady[0].body_master_cycles -ne 42 -or
    [int] $steady[0].write_count -ne 0
) {
    throw "Three-Blobs controller normalized replay sample changed."
}

Write-Output "Three-Blobs controller semantics verified."
Write-Output (
    "Fixed controller: slot 57 installs 03:65C2 before blob slots 58/59/5A."
)
Write-Output (
    "Routed outcomes: 2,565 first-pair mismatches, 35 third-value " +
    "mismatches, and zero equal-state activations."
)
Write-Output (
    "Inactive result contract: all-red awards 100 stored units and sets " +
    "bit 7 of `$53 before the shared randomized reset."
)
Write-Output (
    "First normalized sample: 03:65C2@1, frame 873, 42 cycles, 0 writes."
)
