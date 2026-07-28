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

function Get-RoutineCalls {
    param(
        [object[]] $Calls,
        [string] $Pc
    )

    return @(
        $Calls |
            Where-Object {
                $_.physical_bank -eq "04" -and $_.logical_pc -eq $Pc
            }
    )
}

function Assert-UniformCalls {
    param(
        [object[]] $Calls,
        [string] $Pc,
        [int] $Count,
        [string] $ReturnPc,
        [int64] $Cycles,
        [int] $Writes
    )

    $selected = @(Get-RoutineCalls $Calls $Pc)
    if (
        $selected.Count -ne $Count -or
        @($selected |
            Where-Object {
                $_.return_instruction_pc -ne $ReturnPc -or
                [int64] $_.body_master_cycles -ne $Cycles -or
                [int] $_.write_count -ne $Writes
            }).Count -ne 0
    ) {
        throw "BS-1 routine 04:$Pc call contract changed."
    }
    return $selected
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# BS-1 contains five type-$C2 targets in a shallow arc. All use 04:47B5,
# start with six hits at $3C00,X, and contribute to the five-object objective
# counter initialized by controller 04:44C6.
Assert-RomHex $rom 0x0080AB @"
00 0C B5 47 64 00 36 00 20 80 82 00
00 0D B5 47 78 00 44 00 20 80 82 00
00 0E B5 47 98 00 40 00 20 80 82 00
00 0F B5 47 B8 00 44 00 20 80 82 00
00 10 B5 47 CC 00 36 00 20 80 82 00
"@ "BS-1 five-target layout row"

Assert-RomHex $rom 0x0084C6 @"
A9 05 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 0A 85
CA 4C DB 44 60 AD 80 3C 09 01 8D 80 3C A9 20 20
21 C0 20 18 C0 20 93 C0 A9 07 20 87 C0 A9 01 9D
00 3A A9 3E 9D 00 39 9E 80 39 A9 80 9D 80 30 A9
02 9D 00 38 A9 60 9D 80 31 A9 00 9D 00 32 A9 50
9D 00 33 A9 00 9D 80 33 A9 00 9D 00 34 A9 FE 9D
80 34 A9 04 9D 00 3B A9 08 9D 00 3A 20 18 C0 DE
00 3A D0 1B A9 08 9D 00 3A BD 80 30 49 20 9D 80
30 29 20 D0 0A A9 10 9D 00 3A DE 00 3B F0 01 60
A9 20 20 21 C0 20 18 C0 A9 01 8D BA 2D A5 53 09
02 85 53 4C 27 C0
"@ "BS-1 objective controller and completion latch"

# Initialization installs hit count six, collision selector $10, completion
# selector $81, and an RNG-derived timer in the range $80-$AF. A nonterminal
# contact submits 50 stored score units. The sixth contact submits 200 units,
# decrements $2D2E, and releases the target. The dormant $4805 RNG path
# refreshes an expired timer into $C0-$EF.
Assert-RomHex $rom 0x0087B5 @"
A9 06 9D 00 3C A9 10 9D 00 38 A9 81 9D 00 3D 20
18 C0 20 81 C0 29 2F 18 69 80 9D 00 3B 20 18 C0
20 6F C0 4A A8 B9 2E 48 9D 80 39 BD 00 38 10 1B
29 7F 9D 00 38 DE 00 3C F0 25 A9 32 85 2F 64 30
20 2A C0 A9 36 20 87 C0 20 B9 68 DE 00 3B D0 0E
20 81 C0 29 2F 18 69 C0 9D 00 3B 20 36 48 60 A9
C8 85 2F 64 30 20 2A C0 20 F6 65 A9 06 20 21 C0
20 18 C0 CE 2E 2D 4C 27 C0
"@ "BS-1 six-hit target family"

# Five adjacent support actors award 10 stored units on contact and remain
# available while the objective is nonzero. Once the objective reaches zero,
# their contact path releases them instead.
Assert-RomHex $rom 0x0089D4 @"
BD 00 38 29 7F 9D 00 38 20 18 C0 BD 00 38 30 09
AD 2E 2D D0 03 4C 31 4A 60 A9 0A 85 2F 64 30 20
2A C0 A9 39 8D 6E 2D A9 F8 85 2B A9 F8 85 2C 20
B5 66 BD 80 30 09 40 9D 80 30 A9 01 9D 80 39 A9
80 20 21 C0 AD 2E 2D D0 03 4C 31 4A 20 18 C0 BD
80 30 29 9F 9D 80 30 9E 80 39 4C D4 49 A9 39 8D
6E 2D 20 B1 66 4C 27 C0
"@ "BS-1 five support actors"

# Bank-4 export 1 is the cross-frame result controller. Zero accumulated
# bonus at $2DB7-$2DB9 selects the three failure strings. Nonzero bonus takes
# the YOU GET path; completion latch $2DBA additionally selects the PERFECT /
# SPECIAL POINTS presentation.
Assert-RomHex $rom 0x008000 "4C 06 40 4C 6C 45" `
    "Bank-4 layout and result exports"
Assert-RomHex $rom 0x00856C @"
A9 10 85 2C 20 BD C0 20 93 C0 A9 10 85 2C 20 BD
C0 A9 06 20 87 C0 20 B4 C0 20 48 C0 A9 01 8D 55
2D 20 A3 4B 20 C0 C0 A9 12 85 2F A9 08 85 30 20
60 C0 AD B7 2D 0D B8 2D 0D B9 2D D0 47 A9 5A 85
2F A9 4C 85 30 A9 62 85 35 A9 01 85 36 20 87 4C
A9 69 85 2F A9 4C 85 30 A9 E2 85 35 A9 01 85 36
20 87 4C A9 78 85 2F A9 4C 85 30 A9 62 85 35 A9
02 85 36 20 87 4C 20 B7 C0 20 90 C0 A2 B4 20 3E
47 4C A4 46
"@ "BS-1 zero-bonus failure result branch"
Assert-RomHex $rom 0x0085F0 @"
A9 1E 85 2F A9 4C 85 30 A9 62 85 35 A9 01 85 36
20 87 4C A9 2D 85 2F A9 4C 85 30 A9 E2 85 35 A9
01 85 36 20 87 4C 20 8F 4A 20 B7 C0 20 90 C0 A2
78 20 3E 47 A9 01 8D 54 2D A9 01 8D 55 2D 9C 66
2D 20 0C C0 20 B8 46 20 C4 4D A9 15 85 2F A9 08
85 30 20 60 C0 AD 66 2D F0 E7 AD BA 2D F0 55 A9
3C 85 2F A9 4C 85 30 A9 62 85 35 A9 01 85 36
"@ "BS-1 scored and perfect result selectors"
Assert-RomHex $rom 0x008C20 @"
20 20 59 4F 55 20 47 45 54 20 20 20 FF 20 42 4F
4E 55 53 20 50 4F 49 4E 54 53 20 FF 20 20 20 20
50 45 52 46 45 43 54 20 20 20 FF 53 50 45 43 49
41 4C 20 50 4F 49 4E 54 53 FF 20 20 20 20 53 4F
52 52 59 20 20 20 20 20 FF 59 4F 55 20 4C 4F 53
54 20 59 4F 55 52 20 FF 42 4F 4E 55 53 20 50 4F
49 4E 54 53 20 20 FF
"@ "Bonus result text"
Assert-RomHex $rom 0x008DC4 @"
CE 55 2D F0 01 60 A9 04 8D 55 2D A9 EF 85 2F A9
4D 85 30 EE 67 2D AD 67 2D 29 01 F0 08 A9 0F 85
2F A9 4E 85 30 A9 03 20 5D C0 60
"@ "Bonus result four-tick glyph scheduler"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$states = @(Import-TraceCsv $StatePath)
if (
    $calls.Count -ne 273 -or
    $writes.Count -ne 3894 -or
    $states.Count -ne 1156
) {
    throw "BS-1 dynamic artifact populations changed."
}

$controllerInit = @(
    Assert-UniformCalls $calls "44C6" 1 "C5E9" 213 8
)
$controller = @(
    Assert-UniformCalls $calls "44CE" 2 "44DA" 27 0
)
$targetInit = @(
    Assert-UniformCalls $calls "47B5" 5 "C5E9" 255 10
)
$targetRng = @(
    Assert-UniformCalls $calls "47C7" 5 "C5E9" 561 18
)
$supportInit = @(
    Assert-UniformCalls $calls "49D4" 5 "C5E9" 228 8
)
$support = @(
    Assert-UniformCalls $calls "49DF" 10 "49EC" 48 0
)

if (
    [int] $controllerInit[0].frame -ne 1341 -or
    $controllerInit[0].input_x -ne "2C" -or
    ($controller.frame -join ",") -ne "1342,1403"
) {
    throw "BS-1 controller route changed."
}

$targetSlots = @("34", "35", "50", "51", "52")
$supportSlots = @("53", "54", "55", "56", "57")
if (
    (($targetInit.input_x | Sort-Object) -join ",") -ne
        ($targetSlots -join ",") -or
    (($targetRng.input_x | Sort-Object) -join ",") -ne
        ($targetSlots -join ",") -or
    (($supportInit.input_x | Sort-Object) -join ",") -ne
        ($supportSlots -join ",") -or
    (($support.input_x | Sort-Object -Unique) -join ",") -ne
        ($supportSlots -join ",")
) {
    throw "BS-1 routed slot ownership changed."
}
if (
    ($targetInit.frame | Select-Object -Unique) -ne "1341" -or
    ($targetRng.frame | Select-Object -Unique) -ne "1342" -or
    (($supportInit.frame | Group-Object |
        ForEach-Object { "$($_.Name):$($_.Count)" }) -join ",") -ne
        "1341:3,1342:2" -or
    (($support.frame | Group-Object |
        ForEach-Object { "$($_.Name):$($_.Count)" }) -join ",") -ne
        "1342:5,1403:5"
) {
    throw "BS-1 initializer/update frame population changed."
}

$targetSteady = @(Get-RoutineCalls $calls "47D5")
if (
    $targetSteady.Count -ne 5 -or
    ($targetSteady.frame | Select-Object -Unique) -ne "1403" -or
    (($targetSteady.input_x | Sort-Object) -join ",") -ne
        ($targetSlots -join ",") -or
    ($targetSteady.return_instruction_pc | Select-Object -Unique) -ne
        "4813" -or
    (($targetSteady | Group-Object body_master_cycles |
        Sort-Object { [int64] $_.Name } |
        ForEach-Object { "$($_.Name):$($_.Count)" }) -join ",") -ne
        "765:2,891:2,1638:1" -or
    (($targetSteady | Group-Object write_count |
        Sort-Object { [int] $_.Name } |
        ForEach-Object { "$($_.Name):$($_.Count)" }) -join ",") -ne
        "19:2,22:2,36:1"
) {
    throw "BS-1 collision-free target updates changed."
}

$rngTimers = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "47CF"
        }
)
if (
    $rngTimers.Count -ne 5 -or
    (($rngTimers.value) -join ",") -ne "AD,85,87,8B,84" -or
    @($rngTimers |
        Where-Object {
            $value = [Convert]::ToInt32($_.value, 16)
            $value -lt 0x80 -or $value -gt 0xAF
        }).Count -ne 0
) {
    throw "BS-1 target RNG timers changed."
}

$objectiveWrites = @(
    $states | Where-Object { $_.logical_address -eq "2D2E" }
)
if (
    $objectiveWrites.Count -ne 2 -or
    $objectiveWrites[0].frame -ne "1" -or
    $objectiveWrites[0].value -ne "00" -or
    $objectiveWrites[1].frame -ne "1341" -or
    $objectiveWrites[1].instruction_physical_bank -ne "04" -or
    $objectiveWrites[1].instruction_logical_pc -ne "44C8" -or
    $objectiveWrites[1].value -ne "05"
) {
    throw "BS-1 objective counter timeline changed."
}

foreach ($address in @("2DB7", "2DB8", "2DB9", "2DBA")) {
    $selected = @(
        $states |
            Where-Object {
                $_.logical_address -eq $address -and [int] $_.frame -ge 1341
            }
    )
    if (
        $selected.Count -ne 1 -or
        $selected[0].frame -ne "1341" -or
        $selected[0].value -ne "00" -or
        $selected[0].instruction_physical_bank -ne "00"
    ) {
        throw "BS-1 zero-progress result field $address changed."
    }
}

$resultStart = @(
    $states |
        Where-Object {
            $_.logical_address -eq "2D55" -and
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "458A"
        }
)
# Sixty calls take the zero branch and include nested glyph writes; the other
# 180 are short one-write leaves.
$allGlyphTicks = @(Get-RoutineCalls $calls "4DC4")
$shortGlyphTicks = @(
    $allGlyphTicks |
        Where-Object {
            $_.return_instruction_pc -eq "4DC9" -and
            [int64] $_.body_master_cycles -eq 27 -and
            [int] $_.write_count -eq 1
        }
)
if (
    $resultStart.Count -ne 1 -or
    $resultStart[0].frame -ne "1459" -or
    $resultStart[0].value -ne "01" -or
    $allGlyphTicks.Count -ne 240 -or
    $shortGlyphTicks.Count -ne 180 -or
    [int] $allGlyphTicks[0].frame -ne 1529 -or
    [int] $allGlyphTicks[-1].frame -ne 1768
) {
    throw "BS-1 failure-result presentation timeline changed."
}

$glyphResets = @(
    $states |
        Where-Object {
            $_.logical_address -eq "2D55" -and
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "4DCC"
        }
)
$glyphAdvances = @(
    $states |
        Where-Object {
            $_.logical_address -eq "2D67" -and
            $_.instruction_physical_bank -eq "04" -and
            $_.instruction_logical_pc -eq "4DD7"
        }
)
if (
    $glyphResets.Count -ne 60 -or
    ($glyphResets.value | Select-Object -Unique) -ne "04" -or
    $glyphAdvances.Count -ne 60 -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "3C"
) {
    throw "BS-1 result glyph scheduler changed."
}

Write-Output "BS-1 target-family semantics verified."
Write-Output (
    "Objective row: five six-hit targets at slots 34,35,50,51,52; " +
    "counter 2D2E starts at 5."
)
Write-Output (
    "Target scoring: 50 stored units per nonterminal hit and 200 on the " +
    "sixth hit; completion decrements 2D2E."
)
Write-Output (
    "Support row: five repeatable 10-unit actors at slots 53-57 while " +
    "the objective remains nonzero."
)
Write-Output (
    "Natural forced-entry route: no target contacts, zero bonus/result " +
    "latches, and one 04:456C no-progress failure presentation."
)
Write-Output (
    "Result scheduler: 240 calls over frames 1529-1768 and 60 glyph advances."
)
