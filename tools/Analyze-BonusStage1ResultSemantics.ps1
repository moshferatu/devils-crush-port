# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Scored", "Perfect")]
    [string] $Result,

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
        [string] $Pc
    )

    return @(
        $States |
            Where-Object {
                $_.logical_address -eq $Address -and
                $_.instruction_physical_bank -eq "04" -and
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

# Result export 04:456C branches on the three-byte accumulated bonus. Zero
# selects the failure text; nonzero reaches 45F0 and the scored presentation.
# After the first presentation handshake, $2DBA selects ordinary cleanup or
# the perfect/special-points branch at 464F.
Assert-RomHex $rom 0x00859E @"
AD B7 2D 0D B8 2D 0D B9 2D D0 47
"@ "Bonus accumulated-score selector"
Assert-RomHex $rom 0x008648 @"
F0 E7 AD BA 2D F0 55 A9 3C 85 2F A9 4C 85 30 A9
62 85 35 A9 01 85 36 20 87 4C A9 4B 85 2F A9 4C
85 30 A9 E2 85 35 A9 01 85 36 20 87 4C
"@ "Bonus perfect-presentation selector"

# Perfect setup clears the accumulated bonus and performs 256 additions of
# 1,000 stored units when $2D38 and zero-page $CA are both zero. This yields
# $03E800 = 256,000 stored units on the routed BS-1 seed.
Assert-RomHex $rom 0x008A5B @"
9C B7 2D 9C B8 2D 9C B9 2D AC 38 2D C8 5A 20 71
4A 7A 88 D0 F8 60 A4 CA 18 AD B7 2D 69 E8 8D B7
2D AD B8 2D 69 03 8D B8 2D 62 6D B9 2D 8D B9 2D
88 D0 E5 60
"@ "Bonus perfect-award construction"

# The transfer helper subtracts $01F4 (500 stored units) once per tick and
# submits that same amount through C0C9 -> 00:F55C. The terminal path submits
# the final remainder, clears all three bytes, and advances $2D66.
Assert-RomHex $rom 0x0086B8 @"
CE 54 2D F0 01 60 A9 01 8D 54 2D 38 AD B7 2D E9
F4 85 2E AD B8 2D E9 01 85 2F AD B9 2D E9 00 85
30 30 2B A5 2E 8D B7 2D A5 2F 8D B8 2D A5 30 8D
B9 2D A5 55 48 64 55 A9 F4 85 2F A9 01 85 30 20
C9 C0 A9 28 20 87 C0 68 85 55 20 8F 4A 60 AD B7
2D 85 2F AD B8 2D 85 30 A5 55 48 64 55 20 C9 C0
68 85 55 9C B7 2D 9C B8 2D 9C B9 2D A9 72 85 35
A9 02 85 36 62 20 F8 4A A0 08 A9 0A 20 F8 4A 88
D0 F8 EE 66 2D 60
"@ "Bonus-to-main-score transfer"
Assert-RomHex $rom 0x0020C9 "4C 5C F5" `
    "C0C9 main-score trampoline"
Assert-RomHex $rom 0x008DC4 @"
CE 55 2D F0 01 60 A9 04 8D 55 2D A9 EF 85 2F A9
4D 85 30 EE 67 2D
"@ "Bonus result glyph cadence"

$states = @(Import-TraceCsv $StatePath)
$expectedStateCount = if ($Result -eq "Scored") { 1081 } else { 7191 }
if ($states.Count -ne $expectedStateCount) {
    throw (
        "Expected $expectedStateCount selected writes for $Result, found " +
        "$($states.Count)."
    )
}

$glyphTicks = @(Select-StateWrites $states "2D55" "4DC4")
$glyphResets = @(Select-StateWrites $states "2D55" "4DCC")
$glyphAdvances = @(Select-StateWrites $states "2D67" "4DD7")
$handshakeClear = @(Select-StateWrites $states "2D66" "462E")
$handshakeAdvances = @(Select-StateWrites $states "2D66" "473A")
if (
    $handshakeClear.Count -ne 1 -or
    $handshakeClear[0].frame -ne "1648" -or
    $handshakeClear[0].value -ne "00"
) {
    throw "$Result first result handshake changed."
}

if ($Result -eq "Scored") {
    if (
        $glyphTicks.Count -ne 181 -or
        $glyphResets.Count -ne 46 -or
        $glyphAdvances.Count -ne 46 -or
        $glyphAdvances[0].frame -ne "1529" -or
        $glyphAdvances[0].value -ne "01" -or
        $glyphAdvances[-1].frame -ne "1709" -or
        $glyphAdvances[-1].value -ne "2E" -or
        $handshakeAdvances.Count -ne 1 -or
        $handshakeAdvances[0].frame -ne "1649" -or
        $handshakeAdvances[0].value -ne "01"
    ) {
        throw "BS-1 scored-result presentation cadence changed."
    }

    foreach ($pair in @(
        @("2DB7", "471B"),
        @("2DB8", "471E"),
        @("2DB9", "4721")
    )) {
        $clears = @(Select-StateWrites $states $pair[0] $pair[1])
        if (
            $clears.Count -ne 1 -or
            $clears[0].frame -ne "1649" -or
            $clears[0].value -ne "00"
        ) {
            throw "BS-1 scored-result clear changed for $($pair[0])."
        }
    }
    if (
        @(Select-StateWrites $states "2DB7" "4A79").Count -ne 0 -or
        @(Select-StateWrites $states "2DB7" "46DD").Count -ne 0
    ) {
        throw "BS-1 scored result unexpectedly entered perfect award transfer."
    }
} else {
    if (
        $glyphTicks.Count -ne 874 -or
        $glyphResets.Count -ne 219 -or
        $glyphAdvances.Count -ne 219 -or
        $glyphAdvances[0].frame -ne "1529" -or
        $glyphAdvances[0].value -ne "01" -or
        $glyphAdvances[-1].frame -ne "2401" -or
        $glyphAdvances[-1].value -ne "DB" -or
        $handshakeAdvances.Count -ne 2 -or
        ($handshakeAdvances.frame -join ",") -ne "1649,2342" -or
        ($handshakeAdvances.value -join ",") -ne "01,02"
    ) {
        throw "BS-1 perfect-result presentation cadence changed."
    }

    $perfectLow = @(Select-StateWrites $states "2DB7" "4A79")
    $perfectMiddle = @(Select-StateWrites $states "2DB8" "4A81")
    $perfectHigh = @(Select-StateWrites $states "2DB9" "4A88")
    if (
        $perfectLow.Count -ne 256 -or
        $perfectMiddle.Count -ne 256 -or
        $perfectHigh.Count -ne 256 -or
        $perfectLow[-1].value -ne "00" -or
        $perfectMiddle[-1].value -ne "E8" -or
        $perfectHigh[-1].value -ne "03"
    ) {
        throw "BS-1 perfect award no longer builds 03E800 stored units."
    }

    $transferLow = @(Select-StateWrites $states "2DB7" "46DD")
    $transferMiddle = @(Select-StateWrites $states "2DB8" "46E2")
    $transferHigh = @(Select-StateWrites $states "2DB9" "46E7")
    if (
        $transferLow.Count -ne 512 -or
        $transferMiddle.Count -ne 512 -or
        $transferHigh.Count -ne 512 -or
        $transferLow[0].frame -ne "1830" -or
        $transferLow[0].value -ne "0C" -or
        $transferMiddle[0].value -ne "E6" -or
        $transferHigh[0].value -ne "03" -or
        $transferLow[-1].frame -ne "2341" -or
        $transferLow[-1].value -ne "00" -or
        $transferMiddle[-1].value -ne "00" -or
        $transferHigh[-1].value -ne "00"
    ) {
        throw "BS-1 perfect award transfer changed."
    }

    foreach ($pair in @(
        @("2DB7", "471B"),
        @("2DB8", "471E"),
        @("2DB9", "4721")
    )) {
        $clears = @(Select-StateWrites $states $pair[0] $pair[1])
        if (
            $clears.Count -ne 2 -or
            ($clears.frame -join ",") -ne "1649,2342" -or
            ($clears.value | Select-Object -Unique) -ne "00"
        ) {
            throw "BS-1 perfect-result terminal clear changed for $($pair[0])."
        }
    }
}

Write-Output "BS-1 $Result result semantics verified."
if ($Result -eq "Scored") {
    Write-Output (
        "Scored route: one-byte 2DB7 seed enters 45F0, advances handshake " +
        "2D66 to 1, and bypasses perfect setup."
    )
    Write-Output (
        "Scored presentation: 181 glyph ticks and 46 advances through frame 1709."
    )
} else {
    Write-Output (
        "Perfect route: 2DB7 plus 2DBA seeds enter 45F0 and perfect branch 464F."
    )
    Write-Output (
        "Perfect award: 256 additions build 03E800 stored units; 512 " +
        "transfers of 01F4 drain it to zero."
    )
    Write-Output (
        "Perfect presentation: handshakes 1 then 2, 874 glyph ticks, and " +
        "219 advances through frame 2401."
    )
}
