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

function Assert-DirectWrites {
    param(
        [object[]] $Writes,
        [string] $InstructionPc,
        [string] $Offset,
        [string] $Value,
        [int] $Count,
        [string] $Label
    )

    $matching = @(
        $Writes |
            Where-Object {
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq $InstructionPc -and
                $_.work_ram_offset -eq $Offset -and
                $_.value -eq $Value
            }
    )
    if ($matching.Count -ne $Count) {
        throw "$Label changed: expected $Count writes, found $($matching.Count)."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# These are the final three hidden records in the normal-table layout. They
# carry no world position or visible appearance and all use system type $80.
Assert-RomHex $rom 0x0049B3 @"
00 5C 1F 7B 00 00 FE FF 00 60 80 00
00 5D 6D 7B 00 00 FE FF 00 60 80 00
00 5E A7 7C 00 00 FE FF 00 60 80 04
"@ "Fixed system-controller records"

# Slot $5C installs 03:7B27. Once the scene and presentation gates permit
# work, its eight-frame timer submits selectors $02/$03/$07 through the
# common banked-call wrapper, all to exported address 08:4030.
Assert-RomHex $rom 0x007B1F @"
A9 01 9D 00 3A 20 18 C0 AD C0 2C 10 03 4C 27 C0
AD B9 2B F0 01 60 DE 00 3A D0 32 A9 08 9D 00 3A
A9 02 85 2A A9 30 85 2F A9 08 85 30 20 60 C0 A9
03 85 2A A9 30 85 2F A9 08 85 30 20 60 C0 A9 07
85 2A A9 30 85 2F A9 08 85 30 20 60 C0 60
"@ "Triple banked-call scheduler"

# Slot $5D uses the same gate and period. Each tick advances one phase modulo
# eight, then renders three marker descriptors at $1E16/$1E18/$1E1A. The
# table is the symmetric 0-1-2-3-4-3-2-1 presentation wave.
Assert-RomHex $rom 0x007B6D @"
A9 04 9D 00 3E A9 01 9D 00 3A 20 18 C0 AD C0 2C
10 03 4C 27 C0 AD B9 2B F0 01 60 DE 00 3A F0 03
4C DF 7B A9 08 9D 00 3A FE 80 3A BD 80 3A C9 08
90 03 9E 80 3A BC 80 3A A5 55 D0 00 A9 16 85 2F
A9 1E 85 30 5A 20 42 C0 7A A9 04 20 E0 7B A9 18
85 2F A9 1E 85 30 5A 20 42 C0 7A A9 09 20 E0 7B
A9 1A 85 2F A9 1E 85 30 5A 20 42 C0 7A A9 0A 20
E0 7B 60 5A 48 B9 EE 7B 20 55 59 68 20 5D C0 7A
60 00 01 02 03 04 03 02 01
"@ "Three-marker presentation animator"

# Slot $5E clears the shared response byte and local collision scratch. Its
# nonzero-scratch branch queries the scene, submits command $08, updates
# $2D38, applies response selector $78, and waits on a 16:14 countdown.
Assert-RomHex $rom 0x007CA7 @"
9C 38 2D 64 CD 20 18 C0 AD C0 2C 10 03 4C 27 C0
A5 CC D0 01 60 20 93 C0 A9 08 20 87 C0 A4 CD AD
00 3D 29 F0 19 4A 7D 8D 00 3D 18 A5 CD 69 03 8D
38 2D A9 78 20 21 C0 A5 53 89 02 D0 04 29 08 F0
0B 9E 80 3D AD 00 3D 29 F0 8D 00 3D 20 18 C0 20
90 C0 64 CC A9 10 9D 00 3B A9 0E 9D 80 3B 20 18
C0 A5 CC D0 B0 A5 53 89 02 D0 13 29 08 D0 0F A4
CD AD 00 3D 29 F0 19 4A 7D 8D 00 3D 80 08 AD 00
3D 29 F0 8D 00 3D BD 00 3B D0 03 DE 80 3B 3A 9D
00 3B 1D 80 3B D0 0B AD 00 3D 29 F0 8D 00 3D 4C
A7 7C
"@ "Collision and scene-response controller"

# C060 is a banked-call wrapper, not the accepted audio-command boundary.
# C087 is the separate accepted audio thunk used only by $5E's dormant
# nonzero-scratch branch.
Assert-RomHex $rom 0x002060 "4C 41 E2" "C060 banked-call thunk"
Assert-RomHex $rom 0x002087 "4C FD EC" "C087 audio-command thunk"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 7953 -or
    $writes.Count -ne 135845 -or
    $states.Count -ne 8381
) {
    throw (
        "Expected 7,953 calls, 135,845 scoped writes, and 8,381 " +
        "selected state writes."
    )
}

$expectedInitializers = @(
    [pscustomobject]@{ Sequence = 1; Pc = "7B1F"; Slot = "5C"; Cycles = 213; Writes = 8; OutputA = "7B" },
    [pscustomobject]@{ Sequence = 2; Pc = "7B6D"; Slot = "5D"; Cycles = 234; Writes = 9; OutputA = "7B" },
    [pscustomobject]@{ Sequence = 3; Pc = "7CA7"; Slot = "5E"; Cycles = 219; Writes = 9; OutputA = "7C" }
)
foreach ($expected in $expectedInitializers) {
    $call = $calls[$expected.Sequence - 1]
    if (
        [int] $call.sequence -ne $expected.Sequence -or
        [int] $call.frame -ne 872 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne $expected.Pc -or
        $call.caller_pc -ne "4096" -or
        $call.return_pc -ne "4099" -or
        $call.return_instruction_pc -ne "C5E9" -or
        $call.input_x -ne $expected.Slot -or
        $call.output_x -ne $expected.Slot -or
        $call.output_a -ne $expected.OutputA -or
        [int64] $call.body_master_cycles -ne $expected.Cycles -or
        [int] $call.write_count -ne $expected.Writes
    ) {
        throw "System-controller initializer $($expected.Pc) changed."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "$($expected.Pc) changed MPR$mpr."
        }
    }
}

foreach ($pc in @("7B27", "7B7A", "7CAF")) {
    $steadyCalls = @($calls | Where-Object { $_.logical_pc -eq $pc })
    if ($steadyCalls.Count -ne 2650) {
        throw "Expected 2,650 routed calls to 03:$pc."
    }
}

$firstSteady = @($calls[3], $calls[4], $calls[5])
$expectedSteady = @(
    [pscustomobject]@{ Pc = "7B27"; Slot = "5C"; Cycles = 48 },
    [pscustomobject]@{ Pc = "7B7A"; Slot = "5D"; Cycles = 48 },
    [pscustomobject]@{ Pc = "7CAF"; Slot = "5E"; Cycles = 45 }
)
for ($index = 0; $index -lt $expectedSteady.Count; ++$index) {
    $call = $firstSteady[$index]
    $expected = $expectedSteady[$index]
    if (
        [int] $call.frame -ne 873 -or
        $call.logical_pc -ne $expected.Pc -or
        $call.input_x -ne $expected.Slot -or
        [int64] $call.body_master_cycles -ne $expected.Cycles -or
        [int] $call.write_count -ne 0
    ) {
        throw "First steady sample 03:$($expected.Pc) changed."
    }
}

# Generic dispatch installation must leave the three live entries at
# 03:7B27, 03:7B7A, and 03:7CAF.
$expectedPointers = @{
    "3F5C" = "27"; "3F5D" = "7A"; "3F5E" = "AF"
    "3FDC" = "7B"; "3FDD" = "7B"; "3FDE" = "7C"
}
foreach ($address in $expectedPointers.Keys) {
    $pointerWrites = @(
        $states |
            Where-Object {
                $_.frame -eq "872" -and
                $_.logical_address -eq $address -and
                $_.value -eq $expectedPointers[$address] -and
                $_.instruction_physical_bank -eq "01" -and
                $_.instruction_logical_pc -in @("C5E1", "C5E6")
            }
    )
    if ($pointerWrites.Count -ne 1) {
        throw "Live dispatch pointer $address changed."
    }
}

# Each active $5C tick submits exactly three selectors to the same export.
Assert-DirectWrites $writes "7B41" "002A" "02" 332 "Scheduler selector 02"
Assert-DirectWrites $writes "7B50" "002A" "03" 332 "Scheduler selector 03"
Assert-DirectWrites $writes "7B5F" "002A" "07" 332 "Scheduler selector 07"
foreach ($pc in @("7B45", "7B54", "7B63")) {
    Assert-DirectWrites $writes $pc "002F" "30" 332 "Scheduler export low byte"
}
foreach ($pc in @("7B49", "7B58", "7B67")) {
    Assert-DirectWrites $writes $pc "0030" "08" 332 "Scheduler export bank"
}

# The matching $5D tick walks its phase and submits three fixed marker
# addresses. Direct writes avoid counting interrupt stack activity.
Assert-DirectWrites $writes "7BAB" "002F" "16" 332 "First marker descriptor"
Assert-DirectWrites $writes "7BBD" "002F" "18" 332 "Second marker descriptor"
Assert-DirectWrites $writes "7BCF" "002F" "1A" 332 "Third marker descriptor"
foreach ($pc in @("7BAF", "7BC1", "7BD3")) {
    Assert-DirectWrites $writes $pc "0030" "1E" 332 "Marker descriptor bank"
}

$phaseWrites = @(
    $states |
        Where-Object {
            $_.logical_address -eq "3ADD" -and
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -in @("7B95", "7B9F")
        }
)
if ($phaseWrites.Count -ne 373) {
    throw "Expected 373 direct phase writes, including modulo-eight resets."
}
$firstPhase = @($phaseWrites | Select-Object -First 9)
$expectedPhaseValues = @("01", "02", "03", "04", "05", "06", "07", "08", "00")
$expectedPhaseFrames = @(941, 949, 957, 965, 973, 981, 989, 997, 997)
for ($index = 0; $index -lt $expectedPhaseValues.Count; ++$index) {
    if (
        $firstPhase[$index].value -ne $expectedPhaseValues[$index] -or
        [int] $firstPhase[$index].frame -ne $expectedPhaseFrames[$index]
    ) {
        throw "Three-marker phase wave changed at sample $($index + 1)."
    }
}

# The routed scene never raises $5E's collision scratch. Its shared response
# byte is cleared at startup and again by the initializer, then remains zero.
$responseWrites = @(
    $states | Where-Object { $_.logical_address -eq "2D38" }
)
if (
    $responseWrites.Count -ne 2 -or
    @($responseWrites | Where-Object { $_.value -ne "00" }).Count -ne 0 -or
    @(
        $responseWrites |
            Where-Object {
                $_.frame -eq "872" -and
                $_.instruction_physical_bank -eq "03" -and
                $_.instruction_logical_pc -eq "7CA7"
            }
    ).Count -ne 1
) {
    throw "Dormant collision-response state changed."
}

Write-Output "Fixed system-controller semantics verified."
Write-Output (
    "Hidden records: slots 5C-5E install 03:7B27, 03:7B7A, and 03:7CAF."
)
Write-Output (
    "Eight-frame presentation cadence: 332 triple export submissions and " +
    "332 three-marker updates."
)
Write-Output "Marker phase table: 0-1-2-3-4-3-2-1."
Write-Output (
    "Collision response: slot 5E remains dormant and 2D38 stays zero."
)
Write-Output (
    "First normalized samples: 03:7B27@1 = 48 cycles, " +
    "03:7B7A@1 = 48 cycles, 03:7CAF@1 = 45 cycles."
)
