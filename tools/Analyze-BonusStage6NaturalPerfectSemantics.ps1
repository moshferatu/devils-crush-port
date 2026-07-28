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

# The stage controller owns the two-objective count and enters the shared
# completion coroutine with multiplier 15 only after the count reaches zero.
Assert-RomHex $rom 0x00B1E3 @"
A9 02 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 0F 85 CA
4C DB 44 60
"@ "BS-6 completion controller"

# A terminal contact returns carry from the hit helper, adds 2,000 stored
# units, decrements the shared objective count, and releases the actor.
Assert-RomHex $rom 0x00B2A8 @"
BD 00 38 10 1D 29 7F 9D 00 38 A9 32 85 2F 64 30 20
2A C0 A9 2E 20 87 C0 20 B9 68 DE 00 3C D0 02 38 60
18 60 A9 D0 85 2F A9 07 85 30 20 2A C0 CE 2E 2D 20
F6 65 4C 27 C0
"@ "BS-6 terminal objective contact"

# The shared completion animation is allowed to finish in-stage. Its terminal
# continuation is the gameplay-owned writer of the perfect latch at $2DBA.
Assert-RomHex $rom 0x008555 @"
60 A9 20 20 21 C0 20 18 C0 A9 01 8D BA 2D A5 53 09
02 85 53 4C 27 C0
"@ "Bonus completion animation terminal"

# The result coroutine selects the perfect presentation from $2DBA, then
# builds and transfers the stage multiplier's special award.
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
if ($entries.Count -ne 98 -or $states.Count -ne 2307) {
    throw "BS-6 natural-perfect artifact populations changed."
}

$controllerInit = @(Select-Entries $entries "05" "71E3")
$objectiveInit = @(Select-Entries $entries "05" "71F8")
$controller = @(Select-Entries $entries "05" "71EB")
$contacts = @(Select-Entries $entries "05" "72A8")
$completion = @(Select-Entries $entries "04" "44DB")
$animationReturns = @(Select-Entries $entries "04" "4555")
$result = @(Select-Entries $entries "04" "456C")
$perfect = @(Select-Entries $entries "04" "464F")
if (
    $controllerInit.Count -ne 1 -or
    $controllerInit[0].frame -ne "1242" -or
    $objectiveInit.Count -ne 2 -or
    ($objectiveInit.x -join ",") -ne "34,35" -or
    ($objectiveInit.frame | Select-Object -Unique) -ne "1242" -or
    $controller.Count -ne 3 -or
    ($controller.frame -join ",") -ne "1242,1304,1305" -or
    $contacts.Count -ne 2 -or
    ($contacts.x -join ",") -ne "34,35" -or
    ($contacts.frame | Select-Object -Unique) -ne "1304" -or
    $completion.Count -ne 1 -or
    $completion[0].frame -ne "1305" -or
    $completion[0].a -ne "0F" -or
    $animationReturns.Count -ne 87 -or
    $animationReturns[0].frame -ne "1338" -or
    $animationReturns[-1].frame -ne "1424" -or
    $result.Count -ne 1 -or
    $result[0].frame -ne "1457" -or
    $perfect.Count -ne 1 -or
    $perfect[0].frame -ne "1711"
) {
    throw "BS-6 natural-perfect entry ordering changed."
}

$objectiveInitWrite = @(
    Select-StateWrites $states "2D2E" "05" "71E5"
)
$objectiveDecrements = @(
    Select-StateWrites $states "2D2E" "05" "72D7"
)
if (
    $objectiveInitWrite.Count -ne 1 -or
    $objectiveInitWrite[0].frame -ne "1242" -or
    $objectiveInitWrite[0].value -ne "02" -or
    $objectiveDecrements.Count -ne 2 -or
    ($objectiveDecrements.frame | Select-Object -Unique) -ne "1304" -or
    ($objectiveDecrements.value -join ",") -ne "01,00"
) {
    throw "BS-6 natural objective completion changed."
}

$bonusLow = @(Select-StateWrites $states "2DB7" "00" "F56B")
$bonusMiddle = @(Select-StateWrites $states "2DB8" "00" "F573")
$bonusHigh = @(Select-StateWrites $states "2DB9" "00" "F57A")
if (
    $bonusLow.Count -ne 4 -or
    $bonusMiddle.Count -ne 4 -or
    $bonusHigh.Count -ne 4 -or
    ($bonusLow.value -join ",") -ne "32,02,34,04" -or
    ($bonusMiddle.value -join ",") -ne "00,08,08,10" -or
    ($bonusHigh.value | Select-Object -Unique) -ne "00"
) {
    throw "BS-6 paired terminal-contact score changed."
}

$perfectLatch = @(Select-StateWrites $states "2DBA" "04" "4560")
if (
    $perfectLatch.Count -ne 1 -or
    $perfectLatch[0].frame -ne "1457" -or
    $perfectLatch[0].value -ne "01"
) {
    throw "BS-6 completion animation no longer owns the perfect latch."
}

$awardLow = @(Select-StateWrites $states "2DB7" "04" "4A79")
$awardMiddle = @(Select-StateWrites $states "2DB8" "04" "4A81")
$awardHigh = @(Select-StateWrites $states "2DB9" "04" "4A88")
if (
    $awardLow.Count -ne 15 -or
    $awardMiddle.Count -ne 15 -or
    $awardHigh.Count -ne 15 -or
    $awardLow[-1].value -ne "98" -or
    $awardMiddle[-1].value -ne "3A" -or
    $awardHigh[-1].value -ne "00"
) {
    throw "BS-6 natural perfect award construction changed."
}

$transferLow = @(Select-StateWrites $states "2DB7" "04" "46DD")
$transferMiddle = @(Select-StateWrites $states "2DB8" "04" "46E2")
$transferHigh = @(Select-StateWrites $states "2DB9" "04" "46E7")
$handshakes = @(Select-StateWrites $states "2D66" "04" "473A")
if (
    $transferLow.Count -ne 38 -or
    $transferMiddle.Count -ne 38 -or
    $transferHigh.Count -ne 38 -or
    $transferLow[0].frame -ne "1703" -or
    $transferLow[7].frame -ne "1710" -or
    $transferLow[8].frame -ne "1892" -or
    $transferLow[-1].frame -ne "1921" -or
    $transferLow[-1].value -ne "00" -or
    $transferMiddle[-1].value -ne "00" -or
    $transferHigh[-1].value -ne "00" -or
    $handshakes.Count -ne 2 -or
    ($handshakes.frame -join ",") -ne "1711,1922" -or
    ($handshakes.value -join ",") -ne "01,02"
) {
    throw "BS-6 natural perfect score transfer changed."
}

$glyphTicks = @(Select-StateWrites $states "2D55" "04" "4DC4")
$glyphAdvances = @(Select-StateWrites $states "2D67" "04" "4DD7")
if (
    $glyphTicks.Count -ne 400 -or
    $glyphAdvances.Count -ne 100 -or
    $glyphTicks[0].frame -ne "1583" -or
    $glyphTicks[-1].frame -ne "1982" -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "64"
) {
    throw "BS-6 natural perfect presentation cadence changed."
}

Write-Output "BS-6 natural perfect outcome verified."
Write-Output (
    "Natural completion: paired terminal contacts reduce 2D2E from 2 to " +
    "0; controller 71EB enters 44DB with multiplier 15."
)
Write-Output (
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560; " +
    "no result-boundary latch seed is used."
)
Write-Output (
    "Awards: 4,100 earned units transfer first, then 15,000 special units " +
    "transfer in 30 chunks."
)
