# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $EntryPath,

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
    for ($index = 0; $index -lt $expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}." -f
                ($Offset + $index)
            )
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

function Select-Entries {
    param(
        [object[]] $Entries,
        [string] $Bank,
        [string] $Pc
    )

    return @(
        $Entries |
            Where-Object {
                $_.physical_bank -eq $Bank -and
                $_.logical_pc -eq $Pc
            }
    )
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

# The controller owns three indexed objective flags and enters the shared
# completion coroutine with multiplier 20 after their animated finalizers.
Assert-RomHex $rom 0x0093AC @"
A9 01 8D 2F 2D 8D 30 2D 8D 31 2D 20 18 C0 AD 2F
2D 0D 30 2D 0D 31 2D D0 07 A9 14 85 CA 4C DB 44
60
"@ "BS-3 completion controller"

# Each objective takes eight hits. Its terminal contact adds the ordinary
# 100-unit hit and 1,000-unit destruction awards before an indexed animation
# continuation clears the corresponding controller flag.
Assert-RomHex $rom 0x0095DF @"
9D 00 37 A9 81 9D 00 3D A9 10 9D 00 38 A9 08 9D
00 3C
"@ "BS-3 objective initializer"
Assert-RomHex $rom 0x00966C @"
BD 00 38 10 1D 29 7F 9D 00 38 A9 64 85 2F 64 30
20 2A C0 A9 26 20 87 C0 DE 00 3C D0 02 38 60 20
B9 68 18 60 A9 E8 85 2F A9 03 85 30 20 2A C0 20
F6 65 A9 06 20 21 C0 20 18 C0 BC 00 37 62 99 2F
2D 4C 27 C0
"@ "BS-3 objective hit and destruction"

Assert-RomHex $rom 0x008555 @"
60 A9 20 20 21 C0 20 18 C0 A9 01 8D BA 2D A5 53 09
02 85 53 4C 27 C0
"@ "Bonus completion animation terminal"
Assert-RomHex $rom 0x008648 @"
F0 E7 AD BA 2D F0 55 A9 3C 85 2F A9 4C 85 30 A9
62 85 35 A9 01 85 36 20 87 4C
"@ "Bonus perfect-result selector"
Assert-RomHex $rom 0x008A71 @"
A4 CA 18 AD B7 2D 69 E8 8D B7 2D AD B8 2D 69 03
8D B8 2D 62 6D B9 2D 8D B9 2D 88 D0 E5 60
"@ "Bonus special-award builder"

$entries = @(Import-TraceCsv $EntryPath)
$states = @(Import-TraceCsv $StatePath)
if ($entries.Count -ne 470 -or $states.Count -ne 2421) {
    throw "BS-3 natural-perfect artifact populations changed."
}

$expectedSlots = @("51", "52", "53")
$controllerInit = @(Select-Entries $entries "04" "53AC")
$objectiveInit = @(Select-Entries $entries "04" "55DF")
$controller = @(Select-Entries $entries "04" "53BA")
$targetUpdates = @(Select-Entries $entries "04" "54E7")
$objectiveUpdates = @(Select-Entries $entries "04" "5626")
$contacts = @(Select-Entries $entries "04" "566C")
$finalizers = @(Select-Entries $entries "04" "56A6")
$completion = @(Select-Entries $entries "04" "44DB")
$animationReturns = @(Select-Entries $entries "04" "4555")
$result = @(Select-Entries $entries "04" "456C")
$perfect = @(Select-Entries $entries "04" "464F")
if (
    $controllerInit.Count -ne 1 -or
    $controllerInit[0].frame -ne "1242" -or
    $objectiveInit.Count -ne 3 -or
    ($objectiveInit.x -join ",") -ne ($expectedSlots -join ",") -or
    ($objectiveInit.frame | Select-Object -Unique) -ne "1242" -or
    $controller.Count -ne 152 -or
    $controller[0].frame -ne "1242" -or
    $controller[-1].frame -ne "1454" -or
    $targetUpdates.Count -ne 215 -or
    $objectiveUpdates.Count -ne 3 -or
    ($objectiveUpdates.x -join ",") -ne ($expectedSlots -join ",") -or
    ($objectiveUpdates.frame | Select-Object -Unique) -ne "1447" -or
    $contacts.Count -ne 3 -or
    ($contacts.x -join ",") -ne ($expectedSlots -join ",") -or
    ($contacts.frame | Select-Object -Unique) -ne "1447" -or
    $finalizers.Count -ne 3 -or
    ($finalizers.x -join ",") -ne ($expectedSlots -join ",") -or
    ($finalizers.frame | Select-Object -Unique) -ne "1453" -or
    $completion.Count -ne 1 -or
    $completion[0].frame -ne "1454" -or
    $completion[0].a -ne "14" -or
    $animationReturns.Count -ne 87 -or
    $animationReturns[0].frame -ne "1487" -or
    $animationReturns[-1].frame -ne "1573" -or
    $result.Count -ne 1 -or
    $result[0].frame -ne "1606" -or
    $perfect.Count -ne 1 -or
    $perfect[0].frame -ne "1857"
) {
    throw "BS-3 natural-perfect entry ordering changed."
}

$flagInitializers = @(
    @(Select-StateWrites $states "2D2F" "04" "53AE"),
    @(Select-StateWrites $states "2D30" "04" "53B1"),
    @(Select-StateWrites $states "2D31" "04" "53B4")
)
$flagClears = @(
    @(Select-StateWrites $states "2D2F" "04" "56AA"),
    @(Select-StateWrites $states "2D30" "04" "56AA"),
    @(Select-StateWrites $states "2D31" "04" "56AA")
)
for ($index = 0; $index -lt 3; ++$index) {
    if (
        $flagInitializers[$index].Count -ne 1 -or
        $flagInitializers[$index][0].frame -ne "1242" -or
        $flagInitializers[$index][0].value -ne "01" -or
        $flagClears[$index].Count -ne 1 -or
        $flagClears[$index][0].frame -ne "1453" -or
        $flagClears[$index][0].value -ne "00"
    ) {
        throw "BS-3 natural objective flag $index changed."
    }
}

# One naturally emitted lane-1 target also contacts the ball while the
# objective actors are waiting to enter their active window.
$targetBalance = @(Select-StateWrites $states "2D29" "04" "54FD")
if (
    $targetBalance.Count -ne 1 -or
    $targetBalance[0].frame -ne "1440" -or
    $targetBalance[0].value -ne "04"
) {
    throw "BS-3 natural emitted-target contact changed."
}

$bonusLow = @(Select-StateWrites $states "2DB7" "00" "F56B")
$bonusMiddle = @(Select-StateWrites $states "2DB8" "00" "F573")
$bonusHigh = @(Select-StateWrites $states "2DB9" "00" "F57A")
if (
    $bonusLow.Count -ne 7 -or
    $bonusMiddle.Count -ne 7 -or
    $bonusHigh.Count -ne 7 -or
    ($bonusLow.frame -join ",") -ne
        "1440,1447,1447,1447,1447,1447,1447" -or
    ($bonusLow.value -join ",") -ne "64,C8,B0,14,FC,60,48" -or
    ($bonusMiddle.value -join ",") -ne "00,00,04,05,08,09,0D" -or
    ($bonusHigh.value | Select-Object -Unique) -ne "00"
) {
    throw "BS-3 natural route score no longer totals 3,400 units."
}

$perfectLatch = @(Select-StateWrites $states "2DBA" "04" "4560")
if (
    $perfectLatch.Count -ne 1 -or
    $perfectLatch[0].frame -ne "1606" -or
    $perfectLatch[0].value -ne "01"
) {
    throw "BS-3 completion animation no longer owns the perfect latch."
}

$awardLow = @(Select-StateWrites $states "2DB7" "04" "4A79")
$awardMiddle = @(Select-StateWrites $states "2DB8" "04" "4A81")
$awardHigh = @(Select-StateWrites $states "2DB9" "04" "4A88")
if (
    $awardLow.Count -ne 20 -or
    $awardMiddle.Count -ne 20 -or
    $awardHigh.Count -ne 20 -or
    $awardLow[-1].value -ne "20" -or
    $awardMiddle[-1].value -ne "4E" -or
    $awardHigh[-1].value -ne "00"
) {
    throw "BS-3 natural perfect award construction changed."
}

$transferLow = @(Select-StateWrites $states "2DB7" "04" "46DD")
$transferMiddle = @(Select-StateWrites $states "2DB8" "04" "46E2")
$transferHigh = @(Select-StateWrites $states "2DB9" "04" "46E7")
$handshakes = @(Select-StateWrites $states "2D66" "04" "473A")
if (
    $transferLow.Count -ne 46 -or
    $transferMiddle.Count -ne 46 -or
    $transferHigh.Count -ne 46 -or
    $transferLow[0].frame -ne "1851" -or
    $transferLow[5].frame -ne "1856" -or
    $transferLow[6].frame -ne "2038" -or
    $transferLow[-1].frame -ne "2077" -or
    $transferLow[-1].value -ne "00" -or
    $transferMiddle[-1].value -ne "00" -or
    $transferHigh[-1].value -ne "00" -or
    $handshakes.Count -ne 2 -or
    ($handshakes.frame -join ",") -ne "1857,2078" -or
    ($handshakes.value -join ",") -ne "01,02"
) {
    throw "BS-3 natural perfect score transfer changed."
}

$glyphTicks = @(Select-StateWrites $states "2D55" "04" "4DC4")
$glyphAdvances = @(Select-StateWrites $states "2D67" "04" "4DD7")
if (
    $glyphTicks.Count -ne 408 -or
    $glyphAdvances.Count -ne 102 -or
    $glyphTicks[0].frame -ne "1731" -or
    $glyphTicks[-1].frame -ne "2138" -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "66"
) {
    throw "BS-3 natural perfect presentation cadence changed."
}

Write-Output "BS-3 natural perfect outcome verified."
Write-Output (
    "Natural completion: three terminal contacts and indexed finalizers " +
    "clear 2D2F-2D31; controller 53BA enters 44DB with multiplier 20."
)
Write-Output (
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560; " +
    "no objective-flag or result-boundary latch seed is used."
)
Write-Output (
    "Awards: 3,400 earned units include one natural emitted-target contact, " +
    "then 20,000 special units transfer in 40 chunks."
)
