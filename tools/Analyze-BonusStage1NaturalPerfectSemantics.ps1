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

# The BS-1 controller owns a count of five and starts the shared completion
# coroutine with multiplier 10 only after every target has released.
Assert-RomHex $rom 0x0084C6 @"
A9 05 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 0A 85
CA 4C DB 44 60
"@ "BS-1 objective controller"

# Each target starts with six hits. Its terminal branch adds 200 stored units,
# enters the shared destruction response, and decrements the objective count.
Assert-RomHex $rom 0x0087B5 @"
A9 06 9D 00 3C A9 10 9D 00 38 A9 81 9D 00 3D 20
18 C0 20 81 C0 29 2F 18 69 80 9D 00 3B 20 18 C0
20 6F C0 4A A8 B9 2E 48 9D 80 39
"@ "BS-1 target initializer"
Assert-RomHex $rom 0x008814 @"
A9 C8 85 2F 64 30 20 2A C0 20 F6 65 A9 06 20 21
C0 20 18 C0 CE 2E 2D 4C 27 C0
"@ "BS-1 terminal target contact"

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
if ($entries.Count -ne 110 -or $states.Count -ne 2202) {
    throw "BS-1 natural-perfect artifact populations changed."
}

$expectedSlots = @("34", "35", "50", "51", "52")
$controllerInit = @(Select-Entries $entries "04" "44C6")
$controller = @(Select-Entries $entries "04" "44CE")
$targetInit = @(Select-Entries $entries "04" "47B5")
$contacts = @(Select-Entries $entries "04" "47D5")
$completion = @(Select-Entries $entries "04" "44DB")
$animationReturns = @(Select-Entries $entries "04" "4555")
$result = @(Select-Entries $entries "04" "456C")
$perfect = @(Select-Entries $entries "04" "464F")
if (
    $controllerInit.Count -ne 1 -or
    $controllerInit[0].frame -ne "1241" -or
    $targetInit.Count -ne 5 -or
    ($targetInit.x -join ",") -ne ($expectedSlots -join ",") -or
    ($targetInit.frame | Select-Object -Unique) -ne "1241" -or
    $controller.Count -ne 9 -or
    ($controller.frame -join ",") -ne
        "1242,1303,1304,1305,1306,1307,1308,1309,1310" -or
    $contacts.Count -ne 5 -or
    ($contacts.x -join ",") -ne ($expectedSlots -join ",") -or
    ($contacts.frame | Select-Object -Unique) -ne "1303" -or
    $completion.Count -ne 1 -or
    $completion[0].frame -ne "1310" -or
    $completion[0].a -ne "0A" -or
    $animationReturns.Count -ne 87 -or
    $animationReturns[0].frame -ne "1343" -or
    $animationReturns[-1].frame -ne "1429" -or
    $result.Count -ne 1 -or
    $result[0].frame -ne "1462" -or
    $perfect.Count -ne 1 -or
    $perfect[0].frame -ne "1710"
) {
    throw "BS-1 natural-perfect entry ordering changed."
}

$objectiveInit = @(Select-StateWrites $states "2D2E" "04" "44C8")
$objectiveDecrements = @(
    Select-StateWrites $states "2D2E" "04" "4828"
)
if (
    $objectiveInit.Count -ne 1 -or
    $objectiveInit[0].frame -ne "1241" -or
    $objectiveInit[0].value -ne "05" -or
    $objectiveDecrements.Count -ne 5 -or
    ($objectiveDecrements.frame | Select-Object -Unique) -ne "1309" -or
    ($objectiveDecrements.value -join ",") -ne "04,03,02,01,00"
) {
    throw "BS-1 five-target objective completion changed."
}

$bonusLow = @(Select-StateWrites $states "2DB7" "00" "F56B")
$bonusMiddle = @(Select-StateWrites $states "2DB8" "00" "F573")
$bonusHigh = @(Select-StateWrites $states "2DB9" "00" "F57A")
if (
    $bonusLow.Count -ne 5 -or
    $bonusMiddle.Count -ne 5 -or
    $bonusHigh.Count -ne 5 -or
    ($bonusLow.frame | Select-Object -Unique) -ne "1303" -or
    ($bonusLow.value -join ",") -ne "C8,90,58,20,E8" -or
    ($bonusMiddle.value -join ",") -ne "00,01,02,03,03" -or
    ($bonusHigh.value | Select-Object -Unique) -ne "00"
) {
    throw "BS-1 natural route score no longer totals 1,000 units."
}

$perfectLatch = @(Select-StateWrites $states "2DBA" "04" "4560")
if (
    $perfectLatch.Count -ne 1 -or
    $perfectLatch[0].frame -ne "1462" -or
    $perfectLatch[0].value -ne "01"
) {
    throw "BS-1 completion animation no longer owns the perfect latch."
}

$awardLow = @(Select-StateWrites $states "2DB7" "04" "4A79")
$awardMiddle = @(Select-StateWrites $states "2DB8" "04" "4A81")
$awardHigh = @(Select-StateWrites $states "2DB9" "04" "4A88")
if (
    $awardLow.Count -ne 10 -or
    $awardMiddle.Count -ne 10 -or
    $awardHigh.Count -ne 10 -or
    $awardLow[-1].value -ne "10" -or
    $awardMiddle[-1].value -ne "27" -or
    $awardHigh[-1].value -ne "00"
) {
    throw "BS-1 natural perfect award construction changed."
}

$transferLow = @(Select-StateWrites $states "2DB7" "04" "46DD")
$transferMiddle = @(Select-StateWrites $states "2DB8" "04" "46E2")
$transferHigh = @(Select-StateWrites $states "2DB9" "04" "46E7")
$handshakes = @(Select-StateWrites $states "2D66" "04" "473A")
if (
    $transferLow.Count -ne 22 -or
    $transferMiddle.Count -ne 22 -or
    $transferHigh.Count -ne 22 -or
    $transferLow[0].frame -ne "1708" -or
    $transferLow[1].frame -ne "1709" -or
    $transferLow[2].frame -ne "1891" -or
    $transferLow[-1].frame -ne "1910" -or
    $transferLow[-1].value -ne "00" -or
    $transferMiddle[-1].value -ne "00" -or
    $transferHigh[-1].value -ne "00" -or
    $handshakes.Count -ne 2 -or
    ($handshakes.frame -join ",") -ne "1710,1911" -or
    ($handshakes.value -join ",") -ne "01,02"
) {
    throw "BS-1 natural perfect score transfer changed."
}

$glyphTicks = @(Select-StateWrites $states "2D55" "04" "4DC4")
$glyphAdvances = @(Select-StateWrites $states "2D67" "04" "4DD7")
if (
    $glyphTicks.Count -ne 384 -or
    $glyphAdvances.Count -ne 96 -or
    $glyphTicks[0].frame -ne "1588" -or
    $glyphTicks[-1].frame -ne "1971" -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "60"
) {
    throw "BS-1 natural perfect presentation cadence changed."
}

Write-Output "BS-1 natural perfect outcome verified."
Write-Output (
    "Natural completion: five terminal target contacts reduce 2D2E from " +
    "five to zero; controller 44CE enters 44DB with multiplier 10."
)
Write-Output (
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560; " +
    "no objective-count, accumulated-score, or result-latch seed is used."
)
Write-Output (
    "Awards: 1,000 earned units transfer first, then 10,000 special units " +
    "transfer in 20 chunks."
)
