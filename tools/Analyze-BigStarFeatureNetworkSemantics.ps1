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

# Slots $45/$48 are the left/right pair controllers, $46/$47/$49/$4A are
# the guide's two pairs of Little Skulls, and central slot $4B is the Big
# Star. Hidden slot $44 owns the shared transition latch.
Assert-RomHex $rom 0x004893 @"
00 44 55 44 00 00 FE FF 00 E0 80 00
00 45 85 44 00 00 3E 00 00 E2 80 00
00 46 E4 44 6C 00 3E 00 24 82 82 00
00 47 E4 44 7C 00 3E 00 24 82 82 00
00 48 85 44 00 00 3E 00 00 E2 80 01
00 49 E4 44 C2 00 3E 00 24 82 82 01
00 4A E4 44 D2 00 3E 00 24 82 82 01
00 4B 5C 56 90 00 7E 00 23 43 80 00
"@ "Little-Skull and Big-Star fixed records"

# The remaining even-direction consumers are the centered Undead Head and
# the two Side Worms. Their layout positions and selector fields complete the
# eight-direction network around the already mapped Four-Star Wall Pockets.
Assert-RomHex $rom 0x0048FF @"
00 4D A0 61 98 00 41 00 25 82 82 00
00 4E 4B 5C 20 00 5E 00 0D CA 83 00
00 4F 7C 5C 00 01 5E 00 12 CA 83 00
"@ "Undead-Head and Side-Worm fixed records"

# The hidden controller clears transition latch $2CFA. Its live state is a
# zero-write leaf while the latch is clear; a set latch starts a 32:28
# countdown and eventually returns to initialization.
Assert-RomHex $rom 0x004455 @"
9C FA 2C 20 18 C0 AD FA 2C D0 01 60 A9 20 9D 00
3B A9 1C 9D 80 3B 20 18 C0 AD FA 2C F0 E2 BD 00
3B D0 03 DE 80 3B 3A 9D 00 3B 1D 80 3B F0 D1 60
"@ "Big-Star transition-latch controller"

# Each pair controller owns one live count at $2CF5/Y and one regeneration
# flag at $2CF8/Y. A zero count raises bit 0 or 1 in $2D04, then runs the
# pair-reset timer and marks its pair available again.
Assert-RomHex $rom 0x004485 @"
9D 00 3E A8 62 99 F5 2C BC 00 3E B9 F8 2C 29 FE
99 F8 2C 20 18 C0 BC 00 3E B9 F5 2C F0 01 60 A9
01 BC 00 3E F0 01 1A 0D 04 2D 8D 04 2D 9E 00 3B
A9 01 9D 80 3B 20 18 C0 BD 00 3B D0 03 DE 80 3B
3A 9D 00 3B 1D 80 3B F0 01 60 BC 00 3E B9 F8 2C
09 01 99 F8 2C A9 03 20 21 C0 20 18 C0 80 A9
"@ "Little-Skull pair controllers"

# A Little Skull increments its pair count at initialization. Collision
# submits $14 stored units (2,000 displayed points), creates the default
# command-$2D impact burst, disables the actor, and decrements that count.
# The pair flag later reactivates the actor.
Assert-RomHex $rom 0x0044E4 @"
9D 00 3E A9 10 9D 00 38 A9 03 9D 00 3D A9 34 9D
00 33 A9 00 9D 80 33 A9 06 9D 00 3B BC 00 3E B9
F5 2C 1A 99 F5 2C AD FA 2C F0 03 4C 8A 45 20 18
C0 18 BD 00 33 69 02 9D 00 33 62 7D 80 33 9D 80
33 DE 00 3B F0 01 60 20 18 C0 AD FA 2C F0 03 4C
8A 45 BD 00 38 30 01 60 A9 14 85 2F 64 30 20 2A
C0 20 EC 41 BD 80 30 09 60 9D 80 30 A9 34 9D 00
33 A9 00 9D 80 33 BC 00 3E B9 F5 2C 3A 99 F5 2C
20 18 C0 BC 00 3E B9 F8 2C 29 01 D0 01 60 BD 80
30 29 9F 9D 80 30 BD 00 38 29 7F 9D 00 38 A9 06
9D 00 3B 4C 00 45
"@ "Little-Skull live, hit, and regeneration states"
Assert-RomHex $rom 0x00202A "4C 4F F5" "Score-entry trampoline"
Assert-RomHex $rom 0x0041EC @"
64 2B 64 2C A9 2D 8D 6E 2D A9 E1 85 2D A9 42 85 2E
"@ "Default impact-burst selection"

# While $2CFA is set, a skull uses its gray/inactive presentation. When the
# latch clears, it reactivates and chooses one of the two $2D04 direction
# bits through the binary RNG result.
Assert-RomHex $rom 0x00458A @"
A9 0B 9D 00 3D BD 80 30 09 40 9D 80 30 A9 38 9D
00 33 A9 00 9D 80 33 20 18 C0 AD FA 2C F0 01 60
BD 80 30 29 BF 9D 80 30 BD 00 38 29 7F 9D 00 38
A9 03 9D 00 3D A9 04 9D 00 3B A9 01 8D 04 2D 20
81 C0 29 01 D0 03 4C 0A 45 A9 02 8D 04 2D 4C 0A
45
"@ "Little-Skull gray-state release"

# The Big Star begins stopped and unselected. Pending Little-Skull bits are
# consumed at 570A. State 0..6 selects signed angular velocity
# +64,+32,+16,0,-16,-32,-64 and duration 3,6,8,0,8,6,3.
Assert-RomHex $rom 0x00565C @"
A9 01 8D 04 2D A9 80 8D 18 2D 9C 00 2D 9C 01 2D
A9 03 9D 00 3B 20 18 C0
"@ "Big-Star initialization"
Assert-RomHex $rom 0x00570A @"
AD 04 2D 29 01 F0 03 20 22 57 AD 04 2D 29 02 F0
03 20 38 57 9C 04 2D 60 BD 00 3B C9 03 B0 04 A9
03 80 1C BD 00 3B 1A C9 07 90 14 3A 80 11 BD 00
3B C9 04 90 04 A9 03 80 06 BD 00 3B F0 01 3A 9D
00 3B 0A A8 B9 79 57 8D 00 2D B9 7A 57 8D 01 2D
BC 00 3B B9 87 57 9D 00 3A 8D 05 2D F0 04 9C FA
2C 60 AD 18 2D C9 04 F0 F5 A9 01 8D FA 2C 60
"@ "Big-Star Little-Skull input and speed selection"
Assert-RomHex $rom 0x005779 @"
40 00 20 00 10 00 00 00 F0 FF E0 FF C0 FF
03 06 08 00 08 06 03
"@ "Big-Star signed speed and duration tables"

# Rotation advances the eight-state arrow and copies its current zero-based
# selector into $2D18. When stopped on selector 4 (Roman V), the controller
# raises the special-condition/session state used by the Warlock rule.
Assert-RomHex $rom 0x005674 @"
20 0A 57 AD 00 2D 0D 01 2D F0 2D DE 00 3A D0 21
AD 05 2D 9D 00 3A AD 01 2D 10 48 BD 80 39 85 38
1A 29 07 9D 80 39 BD 00 38 09 40 9D 00 38 20 EA
56 A5 CB 29 FD 85 CB 60 A5 CB 29 02 D0 24 AD 00
2D 0D 01 2D D0 EB AD 18 2D C9 04 D0 E4 AD 0F 2D
D0 DF A9 02 85 CA A5 53 09 04 85 53 A5 CB 09 02
85 CB 60
"@ "Big-Star rotation and stopped-V result"
Assert-RomHex $rom 0x0056EA @"
A9 06 85 37 A5 38 20 A5 C0 20 A8 C0 A9 0C 85 37
BD 80 39 8D 18 2D 20 A5 C0 A9 0D 85 37 4C A8 C0
"@ "Big-Star arrow presentation and selector"

# The zero-based selector covers all eight guide numerals: 0 reaches the
# Undead Head, 2/6 the Side Worms, 1/3/5/7 the four Wall Pockets, and 4 the
# stopped-V condition above.
Assert-RomHex $rom 0x0061A0 @"
20 18 C0 AD 00 2D 0D 01 2D D0 07 AD 18 2D C9 00
F0 01 60
"@ "Undead-Head selector-zero gate"
Assert-RomHex $rom 0x005C4B @"
9E 00 35 A9 02 9D 00 37 A9 06 9D 80 37 A9 05 9D
80 3C A9 AF 9D 00 3E A9 5C 9D 80 3E A9 08 9D 00
34 A9 0E 9D 80 34 A9 C5 9D 00 3F A9 5C 9D 80 3F
60
"@ "Left Side-Worm selector-six initialization"
Assert-RomHex $rom 0x005C7C @"
A9 01 9D 00 35 A9 03 9D 00 37 A9 02 9D 80 37 A9
0B 9D 80 3C A9 AF 9D 00 3E A9 5C 9D 80 3E A9 0A
9D 00 34 A9 0E 9D 80 34 A9 C5 9D 00 3F A9 5C 9D
80 3F 60
"@ "Right Side-Worm selector-two initialization"
Assert-RomHex $rom 0x005CC5 @"
AD 00 2D 0D 01 2D D0 08 AD 18 2D DD 80 37 F0 01
60
"@ "Side-Worm stopped-selector gate"
Assert-RomHex $rom 0x004AAB @"
AD 00 2D 0D 01 2D D0 08 AD 18 2D DD 80 37 F0 0F
"@ "Four-Star Wall-Pocket stopped-selector gate"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 2661 -or
    $writes.Count -ne 487 -or
    $states.Count -ne 7106
) {
    throw (
        "Expected 2,661 Big-Star-network calls, 487 scoped writes, and " +
        "7,106 selected state writes."
    )
}

$initializerPcs = @(
    "4455", "4485", "44E4", "44E4", "4485", "44E4",
    "44E4", "565C", "61A0", "5C4B", "5C7C"
)
$initializerBanks = @(
    "02", "02", "02", "02", "02", "02", "02", "02", "03", "02", "02"
)
$initializerSlots = @(
    "44", "45", "46", "47", "48", "49", "4A", "4B", "4D", "4E", "4F"
)
$initializerInputs = @(
    "00", "00", "00", "00", "01", "01", "01", "00", "00", "00", "00"
)
$initializerCycles = @(207, 285, 390, 390, 285, 390, 390, 285, 192, 204, 210)
$initializerWrites = @(8, 10, 14, 14, 10, 14, 14, 12, 7, 10, 10)

$initializers = @($calls | Where-Object { $_.logical_pc -ne "445B" })
if ($initializers.Count -ne 11) {
    throw "Big-Star-network initializer population changed."
}
for ($index = 0; $index -lt $initializers.Count; ++$index) {
    $call = $initializers[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        [int] $call.frame -ne 872 -or
        $call.physical_bank -ne $initializerBanks[$index] -or
        $call.logical_pc -ne $initializerPcs[$index] -or
        $call.input_a -ne $initializerInputs[$index] -or
        $call.input_x -ne $initializerSlots[$index] -or
        [int64] $call.body_master_cycles -ne $initializerCycles[$index] -or
        [int] $call.write_count -ne $initializerWrites[$index] -or
        $call.input_x -ne $call.output_x -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Big-Star-network initializer changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Big-Star network changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

$steady = @($calls | Where-Object { $_.logical_pc -eq "445B" })
$steadyLeaf = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 21 -and
            [int] $_.write_count -eq 0
        }
)
$steadyInterrupts = @($steady | Where-Object { [int] $_.write_count -gt 0 })
if (
    $steady.Count -ne 2650 -or
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600 -or
    ($steady.input_x | Select-Object -Unique) -ne "44" -or
    ($steady.return_instruction_pc | Select-Object -Unique) -ne "4460" -or
    $steadyLeaf.Count -ne 2622 -or
    $steadyInterrupts.Count -ne 28 -or
    @($steady | Where-Object {
        $_.body_master_cycles -eq "678" -and $_.write_count -eq "12"
    }).Count -ne 8 -or
    @($steady | Where-Object {
        $_.body_master_cycles -eq "714" -and $_.write_count -eq "13"
    }).Count -ne 13 -or
    @($steady | Where-Object {
        $_.body_master_cycles -eq "768" -and $_.write_count -eq "14"
    }).Count -ne 4 -or
    @($steady | Where-Object {
        $_.body_master_cycles -eq "774" -and $_.write_count -eq "15"
    }).Count -ne 1 -or
    @($steady | Where-Object {
        $_.body_master_cycles -eq "906" -and $_.write_count -eq "14"
    }).Count -ne 2
) {
    throw "Big-Star transition-latch idle population changed."
}

# Frame-872 initialization establishes two live skulls per pair and an
# unselected, stopped Big Star with its first pending pair-direction bit.
$expectedGlobalWrites = @{
    "448A:2CF5" = "00"
    "448A:2CF6" = "00"
    "4495:2CF8" = "00"
    "4495:2CF9" = "00"
    "4507:2CF5" = "01,02"
    "4507:2CF6" = "01,02"
    "565E:2D04" = "01"
    "5663:2D18" = "80"
    "5666:2D00" = "00"
    "5669:2D01" = "00"
}
foreach ($key in $expectedGlobalWrites.Keys) {
    $parts = $key.Split(":")
    $matching = @(
        $states |
            Where-Object {
                $_.frame -eq "872" -and
                $_.instruction_logical_pc -eq $parts[0] -and
                $_.logical_address -eq $parts[1]
            }
    )
    if (
        $matching.Count -eq 0 -or
        ($matching.value -join ",") -ne $expectedGlobalWrites[$key]
    ) {
        throw "Big-Star-network initialization changed for $key."
    }
}

# The route never camera-dispatches the Little Skulls, Big Star, Undead Head,
# or Side Worms. Thus no bounded scope owns a score or impact activation and
# the only repeated live work is the clear-latch leaf.
$scoreWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "00" -and
            $_.instruction_logical_pc -in @("F587", "F58F", "F597", "F5A5")
        }
)
$impactWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "41F3"
        }
)
if ($scoreWrites.Count -ne 0 -or $impactWrites.Count -ne 0) {
    throw "Big-Star network unexpectedly exercised a hit result."
}

Write-Output "Big-Star feature-network semantics verified."
Write-Output (
    "Fixed family: Little-Skull pairs 46/47 and 49/4A, controllers 45/48, " +
    "Big Star 4B."
)
Write-Output (
    "Selector registry: I=Undead Head, II/IV/VI/VIII=Wall Pockets, " +
    "III/VII=Side Worms, V=Warlock condition."
)
Write-Output (
    "Inactive Little-Skull hit: 14 stored units, command-2D impact, " +
    "pair-count decrement and Big-Star direction bit."
)
Write-Output (
    "Big-Star speeds: +64,+32,+16,0,-16,-32,-64 across states 0-6."
)
Write-Output (
    "Routed coverage: 11 initializers and 2,650 clear-latch calls; " +
    "feature actors remain camera-inactive."
)
Write-Output "First normalized sample: 02:445B@1, frame 873, 21 cycles."
