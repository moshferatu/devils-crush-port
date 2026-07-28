# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $EntryPath,

    [Parameter(Mandatory = $true)]
    [string] $StatePath,

    [switch] $FullEnvelope
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

Assert-RomHex $rom 0x008E8F @"
A9 1C 8D 2E 2D 20 18 C0 AD 2E 2D D0 07 A9 32 85 CA
4C DB 44 60
"@ "BS-2 objective controller"

# The complete live-target state checks stage completion, passes the target
# through shared collision geometry at 04:5A12, updates its presentation,
# and only then branches to the post-collision continuation below.
Assert-RomHex $rom 0x009079 @"
AD 2E 2D D0 03 4C CD 50 BD 00 37 20 12 5A B0 31 20
D8 50 BD 00 38 10 07 29 7F 9D 00 38 80 01 60
"@ "BS-2 complete target contact envelope"

# This is the target's post-collision continuation. It retains the ordinary
# 50-unit award, response, board resolution, lane balance, and actor release.
Assert-RomHex $rom 0x009099 @"
A9 32 85 2F 64 30 20 2A C0 A9 35 8D 6E 2D 20 48
66 20 60 52 BC 80 34 B9 26 2D 3A 99 26 2D 4C 27
C0
"@ "BS-2 post-collision target continuation"

Assert-RomHex $rom 0x009260 @"
38 BD 80 31 E9 60 30 25 4A 4A 4A 4A C9 08 B0 1D
85 26 18 BD 00 33 69 14 38 E9 50 30 10 29 F0 C9
50 B0 0A 4A 18 65 26 A8 B9 4C 53 10 01 60
"@ "BS-2 target-to-board mapping"
Assert-RomHex $rom 0x0092B7 @"
20 ED 52 B0 30 18 A5 2D 69 01 85 2D 62 65 2E 85
2E 20 ED 52 18 A5 2D 69 3F 85 2D 62 65 2E 85 2E
20 ED 52 18 A5 2D 69 01 85 2D 62 65 2E 85 2E 20
ED 52 CE 2E 2D 60
"@ "BS-2 four-tile resolution"
Assert-RomHex $rom 0x00934C @"
00 01 02 03 04 05 06 07
08 FF FF FF FF FF FF 09
0A 0B 0C 0D 0E 0F 10 11
12 FF FF FF FF FF FF 13
14 15 16 17 18 19 1A 1B
"@ "BS-2 28-cell board"

# Collision geometry adjusts the target's 16-bit X/Y coordinates while
# searching the contact surface. The routed collision path takes the
# decrement-X, increment-Y, and decrement-Y instructions below.
Assert-RomHex $rom 0x009AA8 @"
C9 20 D0 0A FE 80 31 D0 03 FE 00 32 80 0B BD 80 31
D0 03 DE 00 32 DE 80 31 B1 2D 29 18 F0 19 C9 08 D0
0A FE 00 33 D0 03 FE 80 33 80 0B BD 00 33 D0 03 DE
80 33 DE 00 33 FE 80 3E 18 60
"@ "BS-2 collision-coordinate scan"

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
$expectedEntryCount = if ($FullEnvelope) { 317 } else { 289 }
$expectedStateCount = if ($FullEnvelope) { 10705 } else { 3463 }
if (
    $entries.Count -ne $expectedEntryCount -or
    $states.Count -ne $expectedStateCount
) {
    throw "BS-2 contact-perfect artifact populations changed."
}

$controllerInit = @(Select-Entries $entries "04" "4E8F")
$controller = @(Select-Entries $entries "04" "4E97")
$envelopes = @(Select-Entries $entries "04" "5079")
$contacts = @(Select-Entries $entries "04" "5099")
$resolutions = @(Select-Entries $entries "04" "5260")
$tileResolutions = @(Select-Entries $entries "04" "52ED")
$completion = @(Select-Entries $entries "04" "44DB")
$animationReturns = @(Select-Entries $entries "04" "4555")
$result = @(Select-Entries $entries "04" "456C")
$perfect = @(Select-Entries $entries "04" "464F")
if (
    $controllerInit.Count -ne 1 -or
    $controllerInit[0].frame -ne "1242" -or
    $controller.Count -ne 30 -or
    $controller[0].frame -ne "1242" -or
    $controller[-1].frame -ne "1332" -or
    (
        $FullEnvelope -and
        (
            $envelopes.Count -ne 28 -or
            $envelopes[0].frame -ne "1304" -or
            $envelopes[-1].frame -ne "1331" -or
            ($envelopes.x | Select-Object -Unique) -ne "53"
        )
    ) -or
    $contacts.Count -ne 28 -or
    $contacts[0].frame -ne "1304" -or
    $contacts[-1].frame -ne "1331" -or
    ($contacts.x | Select-Object -Unique) -ne "53" -or
    $resolutions.Count -ne 28 -or
    $resolutions[0].frame -ne "1304" -or
    $resolutions[-1].frame -ne "1331" -or
    $tileResolutions.Count -ne 112 -or
    $tileResolutions[0].frame -ne "1304" -or
    $tileResolutions[-1].frame -ne "1331" -or
    $completion.Count -ne 1 -or
    $completion[0].frame -ne "1332" -or
    $completion[0].a -ne "32" -or
    $animationReturns.Count -ne 87 -or
    $animationReturns[0].frame -ne "1365" -or
    $animationReturns[-1].frame -ne "1451" -or
    $result.Count -ne 1 -or
    $result[0].frame -ne "1484" -or
    $perfect.Count -ne 1 -or
    $perfect[0].frame -ne "1754"
) {
    throw "BS-2 contact-perfect entry ordering changed."
}

$objectiveInit = @(Select-StateWrites $states "2D2E" "04" "4E91")
$objectiveDecrements = @(
    Select-StateWrites $states "2D2E" "04" "52E9"
)
$expectedObjectiveValues = @(
    27..0 | ForEach-Object { "{0:X2}" -f $_ }
)
if (
    $objectiveInit.Count -ne 1 -or
    $objectiveInit[0].frame -ne "1242" -or
    $objectiveInit[0].value -ne "1C" -or
    $objectiveDecrements.Count -ne 28 -or
    $objectiveDecrements[0].frame -ne "1304" -or
    $objectiveDecrements[-1].frame -ne "1331" -or
    ($objectiveDecrements.value -join ",") -ne
        ($expectedObjectiveValues -join ",")
) {
    throw "BS-2 28-cell objective completion changed."
}

if ($FullEnvelope) {
    $expectedResolvedX = @(
        "60", "70", "80", "90", "A0", "B0", "C0", "D0",
        "60", "D0",
        "60", "70", "80", "90", "A0", "B0", "C0", "D0",
        "60", "D0",
        "60", "70", "80", "90", "A0", "B0", "C0", "D0"
    )
    $expectedResolvedY = @(
        1..8 | ForEach-Object { "3C" }
        1..2 | ForEach-Object { "4C" }
        1..8 | ForEach-Object { "5C" }
        1..2 | ForEach-Object { "6C" }
        1..8 | ForEach-Object { "7C" }
    )
    $expectedForwardY = @(
        1..8 | ForEach-Object { "74" }
        1..2 | ForEach-Object { "84" }
        1..8 | ForEach-Object { "94" }
        1..2 | ForEach-Object { "A4" }
        1..8 | ForEach-Object { "B4" }
    )
    $xScans = @(
        Select-StateWrites $states "31D3" "04" "5ABE"
    )
    $yForwardScans = @(
        Select-StateWrites $states "3353" "04" "5ACB"
    )
    $yBackwardScans = @(
        Select-StateWrites $states "3353" "04" "5ADD"
    )
    $xGroups = @($xScans | Group-Object frame)
    $yForwardGroups = @($yForwardScans | Group-Object frame)
    $yBackwardGroups = @($yBackwardScans | Group-Object frame)
    if (
        $xScans.Count -ne 4032 -or
        $yForwardScans.Count -ne 1568 -or
        $yBackwardScans.Count -ne 1568 -or
        $xGroups.Count -ne 28 -or
        $yForwardGroups.Count -ne 28 -or
        $yBackwardGroups.Count -ne 28 -or
        @($xGroups | Where-Object { $_.Count -ne 144 }).Count -ne 0 -or
        @($yForwardGroups |
            Where-Object { $_.Count -ne 56 }).Count -ne 0 -or
        @($yBackwardGroups |
            Where-Object { $_.Count -ne 56 }).Count -ne 0 -or
        (($xGroups |
            ForEach-Object { $_.Group[-1].value }) -join ",") -ne
            ($expectedResolvedX -join ",") -or
        (($yForwardGroups |
            ForEach-Object { $_.Group[-1].value }) -join ",") -ne
            ($expectedForwardY -join ",") -or
        (($yBackwardGroups |
            ForEach-Object { $_.Group[-1].value }) -join ",") -ne
            ($expectedResolvedY -join ",")
    ) {
        throw "BS-2 pre-contact collision-coordinate scan changed."
    }
}

$laneBalances = @(Select-StateWrites $states "2D26" "04" "50B4")
if (
    $laneBalances.Count -ne 28 -or
    $laneBalances[0].frame -ne "1304" -or
    $laneBalances[-1].frame -ne "1331" -or
    ($laneBalances.value | Select-Object -Unique) -ne "00"
) {
    throw "BS-2 contact lane balancing changed."
}

$bonusLow = @(Select-StateWrites $states "2DB7" "00" "F56B")
$bonusMiddle = @(Select-StateWrites $states "2DB8" "00" "F573")
$bonusHigh = @(Select-StateWrites $states "2DB9" "00" "F57A")
$scoreFrameGroups = @($bonusLow | Group-Object frame)
if (
    $bonusLow.Count -ne 140 -or
    $bonusMiddle.Count -ne 140 -or
    $bonusHigh.Count -ne 140 -or
    $scoreFrameGroups.Count -ne 28 -or
    @($scoreFrameGroups | Where-Object { $_.Count -ne 5 }).Count -ne 0 -or
    $bonusLow[-1].value -ne "38" -or
    $bonusMiddle[-1].value -ne "31" -or
    $bonusHigh[-1].value -ne "00"
) {
    throw "BS-2 resolved-board score no longer totals 12,600 units."
}

$perfectLatch = @(Select-StateWrites $states "2DBA" "04" "4560")
if (
    $perfectLatch.Count -ne 1 -or
    $perfectLatch[0].frame -ne "1484" -or
    $perfectLatch[0].value -ne "01"
) {
    throw "BS-2 completion animation no longer owns the perfect latch."
}

$awardLow = @(Select-StateWrites $states "2DB7" "04" "4A79")
$awardMiddle = @(Select-StateWrites $states "2DB8" "04" "4A81")
$awardHigh = @(Select-StateWrites $states "2DB9" "04" "4A88")
if (
    $awardLow.Count -ne 50 -or
    $awardMiddle.Count -ne 50 -or
    $awardHigh.Count -ne 50 -or
    $awardLow[-1].value -ne "50" -or
    $awardMiddle[-1].value -ne "C3" -or
    $awardHigh[-1].value -ne "00"
) {
    throw "BS-2 perfect award construction changed."
}

$transferLow = @(Select-StateWrites $states "2DB7" "04" "46DD")
$transferMiddle = @(Select-StateWrites $states "2DB8" "04" "46E2")
$transferHigh = @(Select-StateWrites $states "2DB9" "04" "46E7")
$handshakes = @(Select-StateWrites $states "2D66" "04" "473A")
if (
    $transferLow.Count -ne 125 -or
    $transferMiddle.Count -ne 125 -or
    $transferHigh.Count -ne 125 -or
    $transferLow[0].frame -ne "1729" -or
    $transferLow[24].frame -ne "1753" -or
    $transferLow[25].frame -ne "1935" -or
    $transferLow[-1].frame -ne "2034" -or
    $transferLow[-1].value -ne "00" -or
    $transferMiddle[-1].value -ne "00" -or
    $transferHigh[-1].value -ne "00" -or
    $handshakes.Count -ne 2 -or
    ($handshakes.frame -join ",") -ne "1754,2035" -or
    ($handshakes.value -join ",") -ne "01,02"
) {
    throw "BS-2 contact-perfect score transfer changed."
}

$glyphTicks = @(Select-StateWrites $states "2D55" "04" "4DC4")
$glyphAdvances = @(Select-StateWrites $states "2D67" "04" "4DD7")
if (
    $glyphTicks.Count -ne 487 -or
    $glyphAdvances.Count -ne 122 -or
    $glyphTicks[0].frame -ne "1609" -or
    $glyphTicks[-1].frame -ne "2095" -or
    $glyphAdvances[0].value -ne "01" -or
    $glyphAdvances[-1].value -ne "7A"
) {
    throw "BS-2 contact-perfect presentation cadence changed."
}

if ($FullEnvelope) {
    Write-Output "BS-2 full-envelope perfect outcome verified."
    Write-Output (
        "Collision envelope: 28 entries at 5079 scan each seed by -144 X " +
        "steps and a balanced 56-step Y sweep before entering 5099."
    )
} else {
    Write-Output "BS-2 contact-driven perfect outcome verified."
}
Write-Output (
    "Board completion: 28 post-collision target continuations resolve all " +
    "112 tiles and reduce 2D2E from 28 to 0."
)
Write-Output (
    "Perfect latch: the 87-step completion animation writes 2DBA at 4560; " +
    "no objective-count, tile-state, or result-latch seed is used."
)
Write-Output (
    "Awards: 12,600 earned units transfer first, then 50,000 special units " +
    "transfer in 100 chunks."
)
