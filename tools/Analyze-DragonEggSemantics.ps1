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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Fixed slot $1F is the invisible group controller. Slots $20-$25 are the
# six Dragon's Egg records directly beneath Dragon's Head.
Assert-RomHex $rom 0x0046D7 @"
00 1F EB 42 00 00 36 02 13 62 80 00
00 20 3F 43 30 00 36 02 13 02 82 00
00 21 3F 43 3A 00 31 02 13 02 82 00
00 22 3F 43 44 00 2C 02 13 02 82 00
00 23 3F 43 28 00 2E 02 13 02 82 00
00 24 3F 43 32 00 29 02 13 02 82 00
00 25 3F 43 3C 00 24 02 13 02 82 00
"@ "Dragon's-Egg layout records"

# The controller clears its reset flag, waits for the six-egg live counter
# at $2CF4 to reach zero, then increments $2CFB, sets bit 0 of $2D20,
# awards 100 stored units, and runs a timed reset sequence. The route does
# not reach that count-zero branch.
Assert-RomHex $rom 0x0042EB @"
AD F7 2C 29 FE 8D F7 2C 20 18 C0 AD F4 2C F0 01 60
EE FB 2C AD 20 2D 09 01 8D 20 2D A9 64 85 2F 64 30
20 2A C0 9E 00 3B A9 01 9D 80 3B 20 18 C0 BD 00 3B
D0 03 DE 80 3B 3A 9D 00 3B 1D 80 3B F0 01 60 AD F7
2C 09 01 8D F7 2C A9 03 20 21 C0 20 18 C0 80 AC
"@ "Dragon's-Egg group controller"

# Each fixed initializer increments the live count. A hit awards 10 stored
# units, selects command $39 for the shared impact burst, creates one dynamic
# child, disables the egg, and decrements the live count.
Assert-RomHex $rom 0x00433F @"
EE F4 2C 20 18 C0
"@ "Dragon's-Egg initializer"
Assert-RomHex $rom 0x004345 @"
BD 00 38 30 01 60 A9 0A 85 2F 64 30 20 2A C0 20 63
C0 BD 80 30 09 40 9D 80 30 A9 39 8D 6E 2D A9 F8 85
2B A9 F8 85 2C 20 FF 41 44 33 A9 01 9D 80 39 20 18
C0 BD 80 30 09 40 9D 80 30 CE F4 2C 20 18 C0 AD F7
2C 29 01 D0 01 60 BD 80 30 29 BF 9D 80 30 BD 00 38
29 7F 9D 00 38 9E 80 39 80 9B
"@ "Dragon's-Egg hit path"

# The child allocator copies the egg position, installs dynamic type $82
# and dispatch $441E, then initializes collision, animation, and movement
# state. The child hit awards 200 stored units, selects command $34 for an
# impact burst, and releases the slot.
Assert-RomHex $rom 0x0043A4 @"
20 24 C0 B0 74 A9 1E 99 00 3F A9 44 99 80 3F A9 82
99 00 30 BD 80 31 99 80 31 BD 00 32 99 00 32 BD 00
33 99 00 33 BD 80 33 99 80 33 A9 28 99 00 39 A9 00
99 80 39 A9 84 99 80 30 A9 13 99 00 38 A9 8A 99 00
3D A9 00 99 00 34 A9 02 99 80 34 A9 00 99 00 35 A9
00 99 80 35 A9 20 99 80 36 A9 02 99 80 38 A5 00 29
01 F0 02 A9 26 99 80 3C A9 04 99 00 3A B9 00 33 99
80 37 60
"@ "Dragon's-Egg hatchling allocator"
Assert-RomHex $rom 0x00441E @"
BD 80 3C 20 66 69 90 03 4C 27 C0 DE 00 3A D0 00 BD
80 3A 9D 80 39 BD 00 38 30 01 60 A9 C8 85 2F 64 30
20 2A C0 20 63 C0 A9 34 8D 6E 2D 20 FF 41 4C 27 C0
"@ "Dragon's-Egg hatchling update"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if (
    $calls.Count -ne 11660 -or
    $writes.Count -ne 2481 -or
    $actorWrites.Count -ne 3893
) {
    throw (
        "Expected 11,660 Dragon's-Egg calls, 2,481 scoped writes, and " +
        "3,893 selected state writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin @(
            "42EB", "42F6", "433F", "4345", "4379", "441E"
        ) -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Dragon's-Egg call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Dragon's-Egg call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "42EB" = 1
    "42F6" = 1790
    "433F" = 6
    "4345" = 9857
    "4379" = 1
    "441E" = 5
} "Dragon's-Egg routine"

$controllerInit = @($calls | Where-Object { $_.logical_pc -eq "42EB" })
$controller = @($calls | Where-Object { $_.logical_pc -eq "42F6" })
$eggInits = @($calls | Where-Object { $_.logical_pc -eq "433F" })
$eggCalls = @($calls | Where-Object { $_.logical_pc -eq "4345" })
$eggTransitions = @($calls | Where-Object { $_.logical_pc -eq "4379" })
$hatchlingCalls = @($calls | Where-Object { $_.logical_pc -eq "441E" })

if (
    $controllerInit[0].frame -ne "871" -or
    $controllerInit[0].input_x -ne "1F" -or
    $controllerInit[0].return_instruction_pc -ne "C5E9" -or
    $controllerInit[0].body_master_cycles -ne "228" -or
    $controllerInit[0].write_count -ne "8" -or
    @($controller | Where-Object {
        $_.input_x -ne "1F" -or
        $_.return_instruction_pc -ne "42FB"
    }).Count -ne 0
) {
    throw "Dragon's-Egg group-controller coverage changed."
}

$expectedEggSlots = @("20", "21", "22", "23", "24", "25")
$expectedEggFrames = @(871, 871, 871, 872, 872, 872)
for ($index = 0; $index -lt 6; ++$index) {
    if (
        $eggInits[$index].input_x -ne $expectedEggSlots[$index] -or
        [int] $eggInits[$index].frame -ne $expectedEggFrames[$index] -or
        $eggInits[$index].return_instruction_pc -ne "C5E9" -or
        $eggInits[$index].body_master_cycles -ne "213" -or
        $eggInits[$index].write_count -ne "8"
    ) {
        throw "Dragon's-Egg initializer $($index + 1) changed."
    }
}

Assert-Histogram $eggCalls "input_x" @{
    "20" = 1790
    "21" = 1796
    "22" = 676
    "23" = 1798
    "24" = 1835
    "25" = 1962
} "Dragon's-Egg fixed slot"
Assert-Histogram $eggCalls "return_instruction_pc" @{
    "434A" = 9856
    "C5E9" = 1
} "Dragon's-Egg fixed return"

$eggHit = @(
    $eggCalls |
        Where-Object { $_.return_instruction_pc -eq "C5E9" }
)
if (
    $eggHit.Count -ne 1 -or
    $eggHit[0].sequence -ne "4730" -or
    $eggHit[0].frame -ne "2425" -or
    $eggHit[0].input_x -ne "22" -or
    $eggHit[0].body_master_cycles -ne "19407" -or
    $eggHit[0].write_count -ne "286"
) {
    throw "Dragon's-Egg routed hit changed."
}
if (
    @(
        $eggCalls |
            Where-Object {
                $_.input_x -eq "22" -and
                [int] $_.frame -gt 2425
            }
    ).Count -ne 0
) {
    throw "Destroyed Dragon's Egg resumed fixed updates."
}
if (
    $eggTransitions.Count -ne 1 -or
    $eggTransitions[0].sequence -ne "4738" -or
    $eggTransitions[0].frame -ne "2426" -or
    $eggTransitions[0].input_x -ne "22" -or
    $eggTransitions[0].return_instruction_pc -ne "C5E9" -or
    $eggTransitions[0].body_master_cycles -ne "249" -or
    $eggTransitions[0].write_count -ne "9"
) {
    throw "Dragon's-Egg disable/count transition changed."
}

$expectedHatchlingFrames = @(2425, 2426, 2427, 2428, 2429)
$expectedHatchlingReturns = @("4439", "4439", "4439", "4439", "C594")
$expectedHatchlingCycles = @(1986, 1863, 1917, 1164, 19965)
$expectedHatchlingWrites = @(38, 35, 36, 22, 384)
for ($index = 0; $index -lt 5; ++$index) {
    $call = $hatchlingCalls[$index]
    if (
        $call.input_x -ne "64" -or
        [int] $call.frame -ne $expectedHatchlingFrames[$index] -or
        $call.return_instruction_pc -ne $expectedHatchlingReturns[$index] -or
        [int64] $call.body_master_cycles -ne $expectedHatchlingCycles[$index] -or
        [int] $call.write_count -ne $expectedHatchlingWrites[$index]
    ) {
        throw "Dragon's-Egg hatchling call $($index + 1) changed."
    }
}

# Initial count 0 -> 6, the routed egg 6 -> 5, and the controller remains
# on its nonzero-count return for every steady call.
$countWrites = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -eq "2CF4" -and
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in @("433F", "4381")
        }
)
if (($countWrites.value -join ",") -ne "01,02,03,04,05,06,05") {
    throw "Dragon's-Egg live-count chronology changed."
}
Assert-Write $writes "02" "42F0" "0CF7" "00" 1 (
    "Dragon's-Egg controller reset flag"
)
if (
    @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -eq "02" -and
                $_.instruction_logical_pc -in @("42FC", "4304", "430D")
            }
    ).Count -ne 0
) {
    throw "Dragon's-Egg all-six completion unexpectedly became active."
}

# The egg and hatchling each own one score award and select the command
# consumed by the shared impact-burst family.
Assert-Write $writes "02" "434D" "002F" "0A" 1 (
    "Dragon's-Egg hit award"
)
Assert-Write $writes "02" "4361" "0D6E" "39" 1 (
    "Dragon's-Egg impact command"
)
Assert-Write $writes "02" "4381" "0CF4" "05" 1 (
    "Dragon's-Egg live-count decrement"
)
Assert-Write $writes "02" "443C" "002F" "C8" 1 (
    "Dragon's-Egg hatchling award"
)
Assert-Write $writes "02" "4448" "0D6E" "34" 1 (
    "Dragon's-Egg hatchling impact command"
)
foreach ($pc in @("F587", "F58F", "F597", "F5A5")) {
    $scoreWrites = @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -eq "00" -and
                $_.instruction_logical_pc -eq $pc
            }
    )
    if ($scoreWrites.Count -ne 2) {
        throw "Dragon's-Egg score write $pc changed."
    }
}

# The single child copies slot $22's (68,556) position and installs
# type $82 / dispatch $441E in dynamic slot $64.
Assert-Write $writes "02" "43AB" "1F64" "1E" 1 (
    "Dragon's-Egg hatchling pointer low"
)
Assert-Write $writes "02" "43B0" "1FE4" "44" 1 (
    "Dragon's-Egg hatchling pointer high"
)
Assert-Write $writes "02" "43B5" "1064" "82" 1 (
    "Dragon's-Egg hatchling type"
)
Assert-Write $writes "02" "43BB" "11E4" "44" 1 (
    "Dragon's-Egg hatchling X low"
)
Assert-Write $writes "02" "43C1" "1264" "00" 1 (
    "Dragon's-Egg hatchling X high"
)
Assert-Write $writes "02" "43C7" "1364" "2C" 1 (
    "Dragon's-Egg hatchling Y low"
)
Assert-Write $writes "02" "43CD" "13E4" "02" 1 (
    "Dragon's-Egg hatchling Y high"
)

$animationWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4429" -and
            $_.work_ram_offset -eq "1A64"
        }
)
if (($animationWrites.value -join ",") -ne "03,02,01,00,FF") {
    throw "Dragon's-Egg hatchling timer chronology changed."
}

$childIdentity = @(
    $actorWrites |
        Where-Object {
            $_.logical_address -in @("3064", "3F64", "3FE4") -and
            [int] $_.frame -ge 2425 -and
            [int] $_.frame -le 2429 -and
            $_.instruction_logical_pc -in @("43AB", "43B0", "43B5", "C581")
        }
)
$expectedChildIdentity = @(
    "2425:43AB:3F64:1E",
    "2425:43B0:3FE4:44",
    "2425:43B5:3064:82",
    "2429:C581:3064:00",
    "2429:C581:3F64:00",
    "2429:C581:3FE4:00"
)
$actualChildIdentity = @(
    $childIdentity |
        ForEach-Object {
            "$($_.frame):$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
if (($actualChildIdentity -join ",") -ne ($expectedChildIdentity -join ",")) {
    throw "Dragon's-Egg hatchling allocation/free identity changed."
}

if (
    $controller[0].frame -ne "873" -or
    $controller[0].body_master_cycles -ne "21" -or
    $controller[0].write_count -ne "0" -or
    $eggCalls[0].frame -ne "873" -or
    $eggCalls[0].body_master_cycles -ne "21" -or
    $eggCalls[0].write_count -ne "0"
) {
    throw "Dragon's-Egg normalized idle samples changed."
}

Write-Output "Dragon's-Egg semantics verified."
Write-Output (
    "Six fixed eggs: slots 20-25, controller slot 1F, " +
    "live count 0 -> 6 -> 5."
)
Write-Output (
    "Routed egg 22 hit: +10 stored units, command 39, " +
    "hatchling slot 64 at 02:441E."
)
Write-Output (
    "Hatchling hit: +200 stored units, command 34, " +
    "release at frame 2429."
)
Write-Output (
    "Inactive all-six completion: +100 stored units and group flag updates " +
    "remain ROM-bounded."
)
Write-Output (
    "Normalized idle samples: 02:42F6@1 and 02:4345@1, " +
    "frame 873, 21 cycles, 0 writes."
)
