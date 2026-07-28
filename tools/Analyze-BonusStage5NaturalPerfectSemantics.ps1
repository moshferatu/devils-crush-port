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

Assert-RomHex $rom 0x00AE0E @"
A9 0F 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 1E 85 CA
4C DB 44 60
"@ "BS-5 completion controller"
Assert-RomHex $rom 0x00B100 @"
A9 10 9D 00 38 A9 84 9D 00 3D A9 04 9D 00 3C 20 18
C0 BD 00 38 10 1D 29 7F 9D 00 38 A9 64 85 2F 64 30
20 2A C0 A9 21 20 87 C0 FE 80 39 BD 80 39 C9 03 F0
01 60 A9 90 85 2F A9 01 85 30 20 2A C0 CE 2E 2D 20
52 66 BD 80 30 09 40 9D 80 30 20 18 C0 AD 2E 2D F0
01 60 20 9E 66 4C 27 C0
"@ "BS-5 three-contact objective"
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
if ($entries.Count -ne 139 -or $states.Count -ne 2722) {
    throw "BS-5 natural-perfect artifact populations changed."
}

$expectedSlots = @(
    "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A",
    "62", "63", "64", "65", "66"
)
$controllerInit = @(Select-Entries $entries "05" "6E0E")
$objectiveEntries = @(Select-Entries $entries "05" "7100")
$controller = @(Select-Entries $entries "05" "6E16")
$updates = @(Select-Entries $entries "05" "7112")
$completion = @(Select-Entries $entries "04" "44DB")
$animationReturns = @(Select-Entries $entries "04" "4555")
$result = @(Select-Entries $entries "04" "456C")
$perfect = @(Select-Entries $entries "04" "464F")
if (
    $controllerInit.Count -ne 1 -or
    $controllerInit[0].frame -ne "1242" -or
    $objectiveEntries.Count -ne 15 -or
    ($objectiveEntries.x -join ",") -ne ($expectedSlots -join ",") -or
    ($objectiveEntries.frame | Select-Object -Unique) -ne "1242" -or
    $controller.Count -ne 3 -or
    ($controller.frame -join ",") -ne "1242,1304,1305" -or
    $updates.Count -ne 30 -or
    (($updates | Select-Object -Last 15).x -join ",") -ne
        ($expectedSlots -join ",") -or
    (($updates | Select-Object -Last 15).frame |
        Select-Object -Unique) -ne "1304" -or
    $completion.Count -ne 1 -or
    $completion[0].frame -ne "1305" -or
    $completion[0].a -ne "1E" -or
    $animationReturns.Count -ne 87 -or
    $animationReturns[0].frame -ne "1338" -or
    $animationReturns[-1].frame -ne "1424" -or
    $result.Count -ne 1 -or
    $result[0].frame -ne "1457" -or
    $perfect.Count -ne 1 -or
    $perfect[0].frame -ne "1718"
) {
    throw "BS-5 natural-perfect entry ordering changed."
}

$objectiveInit = @(Select-StateWrites $states "2D2E" "05" "6E10")
$objectiveDecrements = @(
    Select-StateWrites $states "2D2E" "05" "7140"
)
$expectedDecrements = @(
    "0E", "0D", "0C", "0B", "0A", "09", "08", "07",
    "06", "05", "04", "03", "02", "01", "00"
)
if (
    $objectiveInit.Count -ne 1 -or
    $objectiveInit[0].frame -ne "1242" -or
    $objectiveInit[0].value -ne "0F" -or
    $objectiveDecrements.Count -ne 15 -or
    ($objectiveDecrements.frame | Select-Object -Unique) -ne "1304" -or
    ($objectiveDecrements.value -join ",") -ne
        ($expectedDecrements -join ",")
) {
    throw "BS-5 natural objective completion changed."
}

$bonusLow = @(Select-StateWrites $states "2DB7" "00" "F56B")
$bonusMiddle = @(Select-StateWrites $states "2DB8" "00" "F573")
$bonusHigh = @(Select-StateWrites $states "2DB9" "00" "F57A")
if (
    $bonusLow.Count -ne 30 -or
    $bonusMiddle.Count -ne 30 -or
    $bonusHigh.Count -ne 30 -or
    $bonusLow[-1].value -ne "4C" -or
    $bonusMiddle[-1].value -ne "1D" -or
    $bonusHigh[-1].value -ne "00"
) {
    throw "BS-5 terminal-field score no longer totals 7,500 units."
}

$perfectLatch = @(Select-StateWrites $states "2DBA" "04" "4560")
if (
    $perfectLatch.Count -ne 1 -or
    $perfectLatch[0].frame -ne "1457" -or
    $perfectLatch[0].value -ne "01"
) {
    throw "BS-5 completion animation no longer owns the perfect latch."
}

$awardLow = @(Select-StateWrites $states "2DB7" "04" "4A79")
$awardMiddle = @(Select-StateWrites $states "2DB8" "04" "4A81")
$awardHigh = @(Select-StateWrites $states "2DB9" "04" "4A88")
if (
    $awardLow.Count -ne 30 -or
    $awardMiddle.Count -ne 30 -or
    $awardHigh.Count -ne 30 -or
    $awardLow[-1].value -ne "30" -or
    $awardMiddle[-1].value -ne "75" -or
    $awardHigh[-1].value -ne "00"
) {
    throw "BS-5 natural perfect award construction changed."
}

$transferLow = @(Select-StateWrites $states "2DB7" "04" "46DD")
$transferMiddle = @(Select-StateWrites $states "2DB8" "04" "46E2")
$transferHigh = @(Select-StateWrites $states "2DB9" "04" "46E7")
$handshakes = @(Select-StateWrites $states "2D66" "04" "473A")
if (
    $transferLow.Count -ne 75 -or
    $transferMiddle.Count -ne 75 -or
    $transferHigh.Count -ne 75 -or
    $transferLow[0].frame -ne "1703" -or
    $transferLow[14].frame -ne "1717" -or
    $transferLow[15].frame -ne "1899" -or
    $transferLow[-1].frame -ne "1958" -or
    $transferLow[-1].value -ne "00" -or
    $transferMiddle[-1].value -ne "00" -or
    $transferHigh[-1].value -ne "00" -or
    $handshakes.Count -ne 2 -or
    ($handshakes.frame -join ",") -ne "1718,1959" -or
    ($handshakes.value -join ",") -ne "01,02"
) {
    throw "BS-5 natural perfect score transfer changed."
}

$glyphTicks = @(Select-StateWrites $states "2D55" "04" "4DC4")
$glyphAdvances = @(Select-StateWrites $states "2D67" "04" "4DD7")
if (
    $glyphTicks.Count -ne 437 -or
    $glyphAdvances.Count -ne 110 -or
    $glyphTicks[0].frame -ne "1583" -or
    $glyphTicks[-1].frame -ne "2019" -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "6E"
) {
    throw "BS-5 natural perfect presentation cadence changed."
}

Write-Output "BS-5 natural perfect outcome verified."
Write-Output (
    "Natural completion: 15 terminal contacts reduce 2D2E from 15 to 0; " +
    "controller 6E16 enters 44DB with multiplier 30."
)
Write-Output (
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560; " +
    "no result-boundary latch seed is used."
)
Write-Output (
    "Awards: 7,500 earned units transfer first, then 30,000 special units " +
    "transfer in 60 chunks."
)
