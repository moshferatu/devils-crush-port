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

# The detailed guide describes seven Gray Blobs in the chute at the top of
# the table. Fixed slots $50-$56 are the only seven-record upper-table row,
# use the same initializer, and carry the three left/center/right subtypes.
Assert-RomHex $rom 0x004923 @"
02 50 76 68 28 00 4E 00 41 82 83 00
02 51 76 68 2E 00 42 00 41 82 83 01
02 52 76 68 36 00 32 00 41 82 83 02
02 53 76 68 42 00 22 00 41 82 83 02
02 54 76 68 5A 00 1E 00 41 82 83 02
02 55 76 68 72 00 1E 00 41 82 83 02
02 56 76 68 8A 00 1E 00 41 82 83 02
"@ "Gray-Blob fixed layout row"

# Initialization selects appearance 5 and collision selector $13, preserves
# the original vertical coordinate, installs the movement fields, and enters
# the common live dispatch at 03:68A1.
Assert-RomHex $rom 0x006876 @"
A9 05 9D 00 3D A9 13 9D 00 38 BD 00 33 9D 00 3E
BD 80 33 9D 80 3E A9 20 9D 80 36 9E 00 34 A9 01
9D 80 34 A9 01 9D 00 3A 20 18 C0
"@ "Gray-Blob initializer"

# The live state clears transient actor flags, derives a 64-95 tick movement
# interval from RNG, alternates its appearance every four ticks, applies its
# two-component velocity, and runs the common collision helper.
Assert-RomHex $rom 0x0068A1 @"
BD 80 30 29 9F 9D 80 30 BD 00 38 29 7F 9D 00 38
20 81 C0 29 1F 18 69 40 9D 80 3B 20 18 C0 DE 00
3A D0 0D A9 04 9D 00 3A BD 80 39 49 01 9D 80 39
BD 00 38 10 03 4C 06 69 DE 80 3B D0 27 38 62 FD
00 34 9D 00 34 62 FD 80 34 9D 80 34 A9 06 9D 00
3B 20 18 C0 20 12 C0 BD 00 38 30 09 DE 00 3B D0
03 4C A1 68 60
"@ "Gray-Blob live movement and collision loop"

# A hit raises the one-frame $2CFD family signal, submits $03E8 stored score
# units, creates the already mapped default command-$2D impact burst, disables
# the source actor, and passes $FF to the common collision response.
Assert-RomHex $rom 0x006906 @"
A9 01 8D FD 2C A9 E8 85 2F A9 03 85 30 20 2A C0
20 EC 41 BD 80 30 09 60 9D 80 30 BD 00 3F 9D 00
3E BD 80 3F 9D 80 3E A9 FF 20 21 C0 20 18 C0 BD
00 38 29 7F 9D 00 38 BD 00 3E 9D 00 3F BD 80 3E
9D 80 3F 60
"@ "Gray-Blob hit result"
Assert-RomHex $rom 0x00202A "4C 4F F5" "Score-entry trampoline"
Assert-RomHex $rom 0x0041EC @"
64 2B 64 2C A9 2D 8D 6E 2D A9 E1 85 2D A9 42 85 2E
"@ "Default impact-burst selection"

# The one-frame signal is consumed by the Three-Blobs movement family. Its
# set branch selects the alternate 03:67EC placement/type path, and the
# subsequent 03:681D state continues to gate on the same signal.
Assert-RomHex $rom 0x00676B @"
AD FD 2C F0 03 4C EC 67
"@ "Three-Blobs Gray-Blob signal branch"
Assert-RomHex $rom 0x0067EC @"
68 68 BC 80 3A B9 73 68 9D 00 39 A9 81 9D 00 30
BD 80 3A 0A 0A A8 B9 5B 68 9D 80 31 B9 5C 68 9D
00 32 B9 5D 68 9D 00 33 B9 5E 68 9D 80 33 20 18
C0
"@ "Three-Blobs alternate placement"
Assert-RomHex $rom 0x00681D @"
AD FD 2C F0 06 AD C7 2C D0 01 60
"@ "Three-Blobs signal continuation"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 7 -or
    $writes.Count -ne 105 -or
    $states.Count -ne 2439
) {
    throw (
        "Expected 7 Gray-Blob initializer calls, 105 scoped writes, and " +
        "2,439 selected state writes."
    )
}

$expectedSlots = @("50", "51", "52", "53", "54", "55", "56")
$expectedSubtypes = @("00", "01", "02", "02", "02", "02", "02")
$expectedYLow = @("4E", "42", "32", "22", "1E", "1E", "1E")
for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne 872 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -ne "6876" -or
        $call.caller_pc -ne "4096" -or
        $call.return_pc -ne "4099" -or
        $call.return_instruction_pc -ne "C5E9" -or
        [int64] $call.body_master_cycles -ne 372 -or
        $call.input_a -ne $expectedSubtypes[$index] -or
        $call.input_x -ne $expectedSlots[$index] -or
        $call.output_a -ne "68" -or
        $call.input_x -ne $call.output_x -or
        $call.input_y -ne $call.output_y -or
        $call.input_sp -ne $call.output_sp -or
        [int] $call.write_count -ne 15
    ) {
        throw "Gray-Blob initializer identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Gray Blob changed MPR$mpr at sequence $($index + 1)."
        }
    }

    $direct = @(
        $writes |
            Where-Object {
                [int] $_.call_sequence -eq $index + 1 -and
                $_.instruction_physical_bank -eq "03"
            }
    )
    if (
        $direct.Count -ne 10 -or
        ($direct.instruction_logical_pc -join ",") -ne
            "6878,687D,6883,6889,688E,6891,6896,689B,689E,689E" -or
        $direct[0].value -ne "05" -or
        $direct[1].value -ne "13" -or
        $direct[2].value -ne $expectedYLow[$index] -or
        $direct[3].value -ne "00" -or
        $direct[4].value -ne "20" -or
        $direct[5].value -ne "00" -or
        $direct[6].value -ne "01" -or
        $direct[7].value -ne "01"
    ) {
        throw "Gray-Blob initializer writes changed for slot $($expectedSlots[$index])."
    }
}

$pointerLows = @(
    $states |
        Where-Object {
            $_.frame -eq "872" -and
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E1" -and
            $_.logical_address -in @(
                "3F50", "3F51", "3F52", "3F53", "3F54", "3F55", "3F56"
            )
        }
)
$pointerHighs = @(
    $states |
        Where-Object {
            $_.frame -eq "872" -and
            $_.instruction_physical_bank -eq "01" -and
            $_.instruction_logical_pc -eq "C5E6" -and
            $_.logical_address -in @(
                "3FD0", "3FD1", "3FD2", "3FD3", "3FD4", "3FD5", "3FD6"
            )
        }
)
if (
    $pointerLows.Count -ne 7 -or
    ($pointerLows.value | Select-Object -Unique) -ne "A1" -or
    $pointerHighs.Count -ne 7 -or
    ($pointerHighs.value | Select-Object -Unique) -ne "68"
) {
    throw "Gray-Blob live dispatch installation changed."
}

# The standard census never reaches the upper chute. No hit raises $2CFD and
# the only bounded entries are the seven initializers, leaving 03:68A1 as a
# statically guarded inactive dispatch.
$familySignalWrites = @(
    $states | Where-Object { $_.logical_address -eq "2CFD" }
)
if (
    $familySignalWrites.Count -ne 2019 -or
    ($familySignalWrites.value | Select-Object -Unique) -ne "00" -or
    @($familySignalWrites |
        Where-Object { $_.instruction_logical_pc -eq "CB13" }).Count -ne 2017 -or
    @($familySignalWrites |
        Where-Object { $_.instruction_logical_pc -eq "65BC" }).Count -ne 1 -or
    @($familySignalWrites |
        Where-Object { $_.instruction_logical_pc -eq "C100" }).Count -ne 1
) {
    throw "Gray-Blob routed family-signal timeline changed."
}

Write-Output "Gray-Blob semantics verified."
Write-Output (
    "Fixed upper-chute row: slots 50-56 at x 40,46,54,66,90,114,138."
)
Write-Output (
    "Routed coverage: seven frame-872 initializers; live 03:68A1 remains inactive."
)
Write-Output (
    "Inactive hit contract: 03E8 stored units, default command-2D impact, " +
    "and one-frame 2CFD signal."
)
Write-Output (
    "Three-Blobs coupling: 2CFD selects alternate placement at 03:67EC."
)
Write-Output "First normalized sample: 03:6876@1, frame 872, 372 cycles."
