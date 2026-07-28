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
# completion coroutine with multiplier 20 only after all three clear.
Assert-RomHex $rom 0x00AA10 @"
A9 01 8D 2F 2D 8D 30 2D 8D 31 2D 20 18 C0 AD 2F
2D 0D 30 2D 0D 31 2D D0 07 A9 14 85 CA 4C DB 44
60
"@ "BS-4 completion controller"

# A terminal eighth hit awards the ordinary 100 units, clears the indexed
# objective flag, and adds the 1,000-unit destruction award.
Assert-RomHex $rom 0x00AA31 @"
9D 00 3E A9 08 9D 00 3C A9 10 9D 00 38 A9 81 9D 00
3D 20 18 C0 BD 00 38 10 1B 29 7F 9D 00 38 A9 64 85
2F 64 30 20 2A C0 A9 2E 20 87 C0 DE 00 3C F0 04 20
B9 68 60 BC 00 3E 62 99 2F 2D 20 F6 65 A9 E8 85 2F
A9 03 85 30 20 2A C0 4C 27 C0
"@ "BS-4 objective hit and destruction"

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
if ($entries.Count -ne 103 -or $states.Count -ne 2393) {
    throw "BS-4 natural-perfect artifact populations changed."
}

$expectedSlots = @("34", "52", "56")
$controllerInit = @(Select-Entries $entries "05" "6A10")
$objectiveEntries = @(Select-Entries $entries "05" "6A31")
$controller = @(Select-Entries $entries "05" "6A1E")
$updates = @(Select-Entries $entries "05" "6A46")
$completion = @(Select-Entries $entries "04" "44DB")
$animationReturns = @(Select-Entries $entries "04" "4555")
$result = @(Select-Entries $entries "04" "456C")
$perfect = @(Select-Entries $entries "04" "464F")
if (
    $controllerInit.Count -ne 1 -or
    $controllerInit[0].frame -ne "1242" -or
    $objectiveEntries.Count -ne 3 -or
    ($objectiveEntries.x -join ",") -ne ($expectedSlots -join ",") -or
    ($objectiveEntries.frame | Select-Object -Unique) -ne "1242" -or
    $controller.Count -ne 3 -or
    ($controller.frame -join ",") -ne "1242,1304,1305" -or
    $updates.Count -ne 6 -or
    (($updates | Select-Object -Last 3).x -join ",") -ne
        ($expectedSlots -join ",") -or
    (($updates | Select-Object -Last 3).frame |
        Select-Object -Unique) -ne "1304" -or
    $completion.Count -ne 1 -or
    $completion[0].frame -ne "1305" -or
    $completion[0].a -ne "14" -or
    $animationReturns.Count -ne 87 -or
    $animationReturns[0].frame -ne "1338" -or
    $animationReturns[-1].frame -ne "1424" -or
    $result.Count -ne 1 -or
    $result[0].frame -ne "1457" -or
    $perfect.Count -ne 1 -or
    $perfect[0].frame -ne "1707"
) {
    throw "BS-4 natural-perfect entry ordering changed."
}

$flagInitializers = @(
    @(Select-StateWrites $states "2D2F" "05" "6A12"),
    @(Select-StateWrites $states "2D30" "05" "6A15"),
    @(Select-StateWrites $states "2D31" "05" "6A18")
)
$flagClears = @(
    @(Select-StateWrites $states "2D2F" "05" "6A6B"),
    @(Select-StateWrites $states "2D30" "05" "6A6B"),
    @(Select-StateWrites $states "2D31" "05" "6A6B")
)
for ($index = 0; $index -lt 3; ++$index) {
    if (
        $flagInitializers[$index].Count -ne 1 -or
        $flagInitializers[$index][0].frame -ne "1242" -or
        $flagInitializers[$index][0].value -ne "01" -or
        $flagClears[$index].Count -ne 1 -or
        $flagClears[$index][0].frame -ne "1304" -or
        $flagClears[$index][0].value -ne "00"
    ) {
        throw "BS-4 natural objective flag $index changed."
    }
}

$bonusLow = @(Select-StateWrites $states "2DB7" "00" "F56B")
$bonusMiddle = @(Select-StateWrites $states "2DB8" "00" "F573")
$bonusHigh = @(Select-StateWrites $states "2DB9" "00" "F57A")
if (
    $bonusLow.Count -ne 6 -or
    $bonusMiddle.Count -ne 6 -or
    $bonusHigh.Count -ne 6 -or
    ($bonusLow.value -join ",") -ne "64,4C,B0,98,FC,E4" -or
    ($bonusMiddle.value -join ",") -ne "00,04,04,08,08,0C" -or
    ($bonusHigh.value | Select-Object -Unique) -ne "00"
) {
    throw "BS-4 terminal-objective score no longer totals 3,300 units."
}

$perfectLatch = @(Select-StateWrites $states "2DBA" "04" "4560")
if (
    $perfectLatch.Count -ne 1 -or
    $perfectLatch[0].frame -ne "1457" -or
    $perfectLatch[0].value -ne "01"
) {
    throw "BS-4 completion animation no longer owns the perfect latch."
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
    throw "BS-4 natural perfect award construction changed."
}

$transferLow = @(Select-StateWrites $states "2DB7" "04" "46DD")
$transferMiddle = @(Select-StateWrites $states "2DB8" "04" "46E2")
$transferHigh = @(Select-StateWrites $states "2DB9" "04" "46E7")
$handshakes = @(Select-StateWrites $states "2D66" "04" "473A")
if (
    $transferLow.Count -ne 46 -or
    $transferMiddle.Count -ne 46 -or
    $transferHigh.Count -ne 46 -or
    $transferLow[0].frame -ne "1701" -or
    $transferLow[5].frame -ne "1706" -or
    $transferLow[6].frame -ne "1888" -or
    $transferLow[-1].frame -ne "1927" -or
    $transferLow[-1].value -ne "00" -or
    $transferMiddle[-1].value -ne "00" -or
    $transferHigh[-1].value -ne "00" -or
    $handshakes.Count -ne 2 -or
    ($handshakes.frame -join ",") -ne "1707,1928" -or
    ($handshakes.value -join ",") -ne "01,02"
) {
    throw "BS-4 natural perfect score transfer changed."
}

$glyphTicks = @(Select-StateWrites $states "2D55" "04" "4DC4")
$glyphAdvances = @(Select-StateWrites $states "2D67" "04" "4DD7")
if (
    $glyphTicks.Count -ne 408 -or
    $glyphAdvances.Count -ne 102 -or
    $glyphTicks[0].frame -ne "1581" -or
    $glyphTicks[-1].frame -ne "1988" -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "66"
) {
    throw "BS-4 natural perfect presentation cadence changed."
}

Write-Output "BS-4 natural perfect outcome verified."
Write-Output (
    "Natural completion: three terminal contacts clear 2D2F-2D31; " +
    "controller 6A1E enters 44DB with multiplier 20."
)
Write-Output (
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560; " +
    "no result-boundary latch seed is used."
)
Write-Output (
    "Awards: 3,300 earned units transfer first, then 20,000 special units " +
    "transfer in 40 chunks."
)
