# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(2, 6)]
    [int] $Stage,

    [Parameter(Mandatory = $true)]
    [string] $RomPath,

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

function Select-StateWrites {
    param(
        [object[]] $States,
        [string] $Address,
        [string] $Pc,
        [string] $Bank = "04"
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

function Assert-SingleWrite {
    param(
        [object[]] $States,
        [string] $Address,
        [string] $Bank,
        [string] $Pc,
        [string] $Value,
        [string] $Label
    )

    $writes = @(Select-StateWrites $States $Address $Pc $Bank)
    if (
        $writes.Count -ne 1 -or
        $writes[0].frame -ne "1342" -or
        $writes[0].value -ne $Value
    ) {
        throw "$Label changed."
    }
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Result export 04:456C branches on the three-byte accumulated bonus. A
# nonzero value reaches 45F0. After the first presentation handshake, zero
# completion latch $2DBA selects ordinary cleanup instead of the perfect
# branch at 464F.
Assert-RomHex $rom 0x00859E @"
AD B7 2D 0D B8 2D 0D B9 2D D0 47
"@ "Bonus accumulated-score selector"
Assert-RomHex $rom 0x008648 @"
F0 E7 AD BA 2D F0 55 A9 3C 85 2F A9 4C 85 30 A9
62 85 35 A9 01 85 36 20 87 4C A9 4B 85 2F A9 4C
85 30 A9 E2 85 35 A9 01 85 36 20 87 4C
"@ "Bonus perfect-presentation selector"

# The ordinary result path clears the accumulated bonus. The perfect path's
# builder and frame-paced transfer are guarded here so the dynamic absence
# checks below retain a stable instruction identity.
Assert-RomHex $rom 0x008A5B @"
9C B7 2D 9C B8 2D 9C B9 2D AC 38 2D C8 5A 20 71
4A 7A 88 D0 F8 60
"@ "Bonus perfect-award entry"
Assert-RomHex $rom 0x0086B8 @"
CE 54 2D F0 01 60 A9 01 8D 54 2D 38 AD B7 2D E9
F4
"@ "Bonus perfect-award transfer entry"
Assert-RomHex $rom 0x008DC4 @"
CE 55 2D F0 01 60 A9 04 8D 55 2D A9 EF 85 2F A9
4D 85 30 EE 67 2D
"@ "Bonus result glyph cadence"

$states = @(Import-TraceCsv $StatePath)
$expectedStateCounts = @{
    2 = 1081
    3 = 1089
    4 = 1083
    5 = 1081
    6 = 1081
}
if ($states.Count -ne $expectedStateCounts[$Stage]) {
    throw (
        "Expected $($expectedStateCounts[$Stage]) selected writes for " +
        "BS-$Stage, found $($states.Count)."
    )
}

# Each route must first initialize the selected stage's authentic nonzero
# objective state. This prevents five copies of the shared result branch from
# passing while accidentally loading the same bonus scene.
switch ($Stage) {
    2 {
        Assert-SingleWrite $states "2D2E" "04" "4E91" "1C" `
            "BS-2 28-cell objective"
    }
    3 {
        Assert-SingleWrite $states "2D2F" "04" "53AE" "01" `
            "BS-3 objective 0"
        Assert-SingleWrite $states "2D30" "04" "53B1" "01" `
            "BS-3 objective 1"
        Assert-SingleWrite $states "2D31" "04" "53B4" "01" `
            "BS-3 objective 2"
    }
    4 {
        Assert-SingleWrite $states "2D2F" "05" "6A12" "01" `
            "BS-4 objective 0"
        Assert-SingleWrite $states "2D30" "05" "6A15" "01" `
            "BS-4 objective 1"
        Assert-SingleWrite $states "2D31" "05" "6A18" "01" `
            "BS-4 objective 2"
    }
    5 {
        Assert-SingleWrite $states "2D2E" "05" "6E10" "0F" `
            "BS-5 15-target objective"
    }
    6 {
        Assert-SingleWrite $states "2D2E" "05" "71E5" "02" `
            "BS-6 paired objective"
    }
}

$glyphTicks = @(Select-StateWrites $states "2D55" "4DC4")
$glyphResets = @(Select-StateWrites $states "2D55" "4DCC")
$glyphAdvances = @(Select-StateWrites $states "2D67" "4DD7")
$handshakeClear = @(Select-StateWrites $states "2D66" "462E")
$handshakeAdvance = @(Select-StateWrites $states "2D66" "473A")
$firstGlyphFrames = @{
    2 = 1529
    3 = 1529
    4 = 1528
    5 = 1530
    6 = 1530
}
$firstGlyphFrame = $firstGlyphFrames[$Stage]
$lastGlyphFrame = $firstGlyphFrame + 180
$handshakeClearFrame = $firstGlyphFrame + 119
$handshakeAdvanceFrame = $firstGlyphFrame + 120
if (
    $glyphTicks.Count -ne 181 -or
    $glyphResets.Count -ne 46 -or
    $glyphAdvances.Count -ne 46 -or
    [int] $glyphAdvances[0].frame -ne $firstGlyphFrame -or
    $glyphAdvances[0].value -ne "01" -or
    [int] $glyphAdvances[-1].frame -ne $lastGlyphFrame -or
    $glyphAdvances[-1].value -ne "2E" -or
    $handshakeClear.Count -ne 1 -or
    [int] $handshakeClear[0].frame -ne $handshakeClearFrame -or
    $handshakeClear[0].value -ne "00" -or
    $handshakeAdvance.Count -ne 1 -or
    [int] $handshakeAdvance[0].frame -ne $handshakeAdvanceFrame -or
    $handshakeAdvance[0].value -ne "01"
) {
    throw "BS-$Stage scored-result presentation cadence changed."
}

foreach ($pair in @(
    @("2DB7", "471B"),
    @("2DB8", "471E"),
    @("2DB9", "4721")
)) {
    $clears = @(Select-StateWrites $states $pair[0] $pair[1])
    if (
        $clears.Count -ne 1 -or
        [int] $clears[0].frame -ne $handshakeAdvanceFrame -or
        $clears[0].value -ne "00"
    ) {
        throw "BS-$Stage scored-result clear changed for $($pair[0])."
    }
}

if (
    @(Select-StateWrites $states "2DB7" "4A79").Count -ne 0 -or
    @(Select-StateWrites $states "2DB7" "46DD").Count -ne 0
) {
    throw "BS-$Stage scored result unexpectedly entered perfect award transfer."
}

Write-Output "BS-$Stage scored-result semantics verified."
Write-Output (
    "Route: authentic nonzero objective state plus one stored unit enters " +
    "45F0 and ordinary cleanup 46A4."
)
Write-Output (
    "Presentation: handshake 2D66 advances to 1; 181 glyph ticks and 46 " +
    "advances run through frame $lastGlyphFrame."
)
Write-Output "Perfect award: builder and transfer remain unentered."
