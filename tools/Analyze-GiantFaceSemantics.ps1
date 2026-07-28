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
    [string] $StateWritePath
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

    [byte[]] $expected = @(
        $Hex.Split(
            [char[]] @(" ", "`r", "`n", "`t"),
            [System.StringSplitOptions]::RemoveEmptyEntries
        ) | ForEach-Object { [Convert]::ToByte($_, 16) }
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

function Assert-Histogram {
    param(
        [object[]] $Rows,
        [string] $Property,
        [hashtable] $Expected,
        [string] $Label
    )

    $actual = @{}
    foreach ($group in @($Rows | Group-Object -Property $Property)) {
        $actual[$group.Name] = $group.Count
    }
    if ($actual.Count -ne $Expected.Count) {
        throw "$Label changed population."
    }
    foreach ($key in $Expected.Keys) {
        if (
            -not $actual.ContainsKey($key) -or
            $actual[$key] -ne $Expected[$key]
        ) {
            throw (
                "$Label changed ${key}: expected $($Expected[$key]), " +
                "found $($actual[$key])."
            )
        }
    }
}

function Assert-Call {
    param(
        [object] $Call,
        [int] $Sequence,
        [int] $Frame,
        [string] $Pc,
        [string] $X,
        [string] $Return,
        [int64] $Cycles,
        [int] $Writes,
        [string] $Label
    )

    if (
        [int] $Call.sequence -ne $Sequence -or
        [int] $Call.frame -ne $Frame -or
        $Call.logical_pc -ne $Pc -or
        $Call.input_x -ne $X -or
        $Call.return_instruction_pc -ne $Return -or
        [int64] $Call.body_master_cycles -ne $Cycles -or
        [int] $Call.write_count -ne $Writes
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

# Slot $36 is the central Giant-Face state controller. Slot $37 is its hidden
# form animator, slot $38 is the mouth/bonus entrance, and slot $39 is the
# crown/helmet collision surface.
Assert-RomHex $rom 0x0047EB @"
01 36 28 74 80 00 24 01 00 60 83 00
00 37 6C 76 00 00 24 01 00 E2 83 00
00 38 18 77 90 00 5E 01 0A A2 82 00
00 39 F7 76 78 00 1E 01 1C A2 82 00
"@ "Giant-Face fixed layout records"

# The controller stages twice, restores persisted form $2258, reconstructs
# animation and form fields for states 1-6, resets out-of-range values, and
# finally installs the request-wait state at 03:74D8.
Assert-RomHex $rom 0x007428 @"
20 18 C0 20 18 C0 AD 58 22 D0 03 4C C5 74 9C 0D
2D 9C 0C 2D 9C 0E 2D 9E 00 3E 20 18 C0 20 18 C0
9E 80 3A 9E 00 3E 9E 80 3E AD 58 22 A8 88 D0 03
4C E0 74 A9 01 8D 0E 2D A9 04 9D 00 3E A9 04 8D
0D 2D FE 80 3E 88 D0 03 4C 4C 75 FE 80 3A FE 80
3E 88 D0 03 4C 4C 75 FE 80 3A FE 80 3E BD 80 3A
5A 20 5A C0 7A 88 D0 03 4C A5 75 A9 08 9D 00 3E
A9 08 8D 0D 2D A9 02 8D 0E 2D 5A 20 4F 76 7A FE
80 3E 88 D0 03 4C 4C 75 FE 80 3A FE 80 3E 88 D0
03 4C 6F 75 9E 80 3A 9E 00 3E 9E 80 3E 62 8D 58
22 9C 0D 2D 9C 0C 2D 9C 0E 2D 9E 00 3E 20 18 C0
"@ "Giant-Face staged initializer and persisted-form restoration"

# Request bit 0 wakes the face, sets form 1 and animation mode 1, and clears
# the request. Request bit 1 uses a six-entry form table and six-entry state
# vector to advance the transformation.
Assert-RomHex $rom 0x0074D8 @"
AD 0C 2D 29 01 D0 01 60 9E 00 3E A9 01 9D 00 3A
20 18 C0 DE 00 3A D0 12 A9 04 9D 00 3A FE 00 3E
20 4F 76 BD 00 3E C9 04 F0 01 60 A9 01 8D 58 22
A9 01 8D 0E 2D 9E 80 3A 9E 80 3E AD 0C 2D 29 FE
8D 0C 2D 20 18 C0 AD 0C 2D 29 02 D0 01 60 AD 0C
2D 29 FD 8D 0C 2D BC 80 3E B9 18 76 8D 58 22 BD
80 3E 0A A8 B9 0C 76 85 2F B9 0D 76 85 30 FE 80
3E 6C 2F 20
"@ "Giant-Face wake and transformation-request dispatcher"

# The later transformation handler sets persisted form 6 and opens the mouth
# through $2D21. The closing handler clears animation mode, rebuilds the
# display, and resets the controller. The tables select forms 2,3,4,5,5,7.
Assert-RomHex $rom 0x00754C @"
A9 08 85 35 A9 10 85 36 A9 02 85 37 20 4A 41 A9
06 20 21 C0 20 18 C0 FE 80 3A BD 80 3A 20 5A C0
4C 1B 75 A9 08 85 35 A9 10 85 36 A9 02 85 37 20
4A 41 A9 06 20 21 C0 20 18 C0 FE 80 3A BD 80 3A
20 5A C0 A9 06 8D 58 22 A9 01 8D 21 2D 20 18 C0
AD 21 2D D0 03 4C 26 75 60 A9 08 85 35 A9 10 85
36 A9 02 85 37 20 4A 41 A9 02 8D 0E 2D FE 00 3E
A9 01 9D 00 3A 20 18 C0 DE 00 3A D0 12 A9 04 9D
00 3A FE 00 3E 20 4F 76 BD 00 3E C9 08 F0 01 60
4C 1B 75 A9 08 85 35 A9 10 85 36 A9 02 85 37 20
4A 41 9C 0E 2D A9 06 20 21 C0 20 18 C0 62 9D 80
3A 20 5A C0 9E 00 3E BD 00 3E 20 55 76 4C C5 74
4C 75 4C 75 A5 75 4C 75 6F 75 DF 75 02 03 04 05
05 07
"@ "Giant-Face transformation handlers and form tables"

# These helpers allocate a temporary type-$08 visual actor, follow global form
# index $2D0D, and update the shared display state.
Assert-RomHex $rom 0x00761E @"
20 24 C0 90 01 60 A9 08 99 80 3D A9 43 99 00 3F
A9 76 99 80 3F A9 1E 85 2F A9 1E 85 30 20 42 C0
A9 01 4C 5D C0 20 18 C0 AD 0D 2D 20 55 76 4C 27
C0 BD 00 3E 8D 0D 2D 85 2D A9 10 85 2F A9 1E 85
30 20 42 C0 A5 2D 20 55 59 A9 01 4C 5D C0
"@ "Giant-Face visual-effect helpers"

# Hidden slot $37 animates bounded form ranges for modes $2D0E=1 and 2.
Assert-RomHex $rom 0x00766C @"
A9 01 9D 00 3E A9 08 9D 80 3B A9 01 9D 00 3A 20
18 C0 AD 0E 2D C9 01 F0 01 60 A9 00 9D 80 3A 9D
00 3E A9 03 9D 80 3E 20 18 C0 20 C5 76 AD 0E 2D
D0 03 4C 6C 76 AD 0E 2D C9 02 F0 01 60 A9 03 9D
80 3A 9D 00 3E A9 06 9D 80 3E 20 18 C0 20 C5 76
AD 0E 2D D0 03 4C 6C 76 60 DE 00 3A F0 01 60 A9
08 9D 00 3A FE 80 3A BD 80 3A DD 80 3E 90 06 BD
00 3E 9D 80 3A A9 12 85 2F A9 1E 85 30 20 42 C0
BD 80 3A 20 55 59 A9 01 4C 5D C0
"@ "Giant-Face hidden form animator"

# A crown/helmet hit clears the collision bit, creates the visual effect,
# emits command $20, and awards 3 stored units (300 displayed points).
Assert-RomHex $rom 0x0076F7 @"
20 18 C0 BD 00 38 10 18 29 7F 9D 00 38 20 1E 76
A9 20 20 87 C0 A9 03 85 2F A9 00 85 30 20 2A C0
60
"@ "Giant-Face crown collision rule"

# The closed mouth has the same 300-point collision rule. Once $2D21 is
# nonzero it becomes type $84; an open-mouth capture sets bit 7, awards 100
# units, positions the ball at (144,344), and emits command $41.
Assert-RomHex $rom 0x007718 @"
BD 00 38 29 7F 9D 00 38 9E 80 39 20 18 C0 AD 21
2D D0 1E BD 00 38 10 18 29 7F 9D 00 38 20 1E 76
A9 20 20 87 C0 A9 03 85 2F A9 00 85 30 20 2A C0
60 FE 80 39 BD 00 38 29 7F 9D 00 38 A9 84 9D 00
30 62 20 9F C0 20 18 C0 BD 00 38 30 01 60 AD 21
2D 09 80 8D 21 2D A9 64 85 2F 64 30 20 2A C0 C2
A9 90 99 80 31 A9 00 99 00 32 A9 58 99 00 33 A9
01 99 80 33 A9 08 20 21 C0 20 18 C0 A9 08 9D 00
3A A9 04 8D 80 39 A9 41 20 87 C0
"@ "Giant-Face mouth collision and open bonus entrance"

Assert-RomHex $rom 0x0077A3 @"
20 18 C0 DE 00 3A D0 0F A9 08 9D 00 3A AD 80 39
C9 06 F0 04 EE 80 39 60 AD 80 30 09 20 8D 80 30
A9 10 20 21 C0 20 18 C0 A9 04 85 55 A5 53 09 10
85 53 62 20 A2 C0
"@ "Giant-Face bonus-stage-4 selection"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 5608 -or
    $writes.Count -ne 812 -or
    $stateWrites.Count -ne 127
) {
    throw (
        "Expected 5,608 calls, 812 scoped writes, and 127 selected " +
        "Giant-Face state writes."
    )
}

$allowedPcs = @(
    "7428", "742B", "742E", "74D8", "766C",
    "767E", "76F7", "76FA", "7718", "7726"
)
$expectedSlots = @{
    "7428" = "36"
    "742B" = "36"
    "742E" = "36"
    "74D8" = "36"
    "766C" = "37"
    "767E" = "37"
    "76F7" = "39"
    "76FA" = "39"
    "7718" = "38"
    "7726" = "38"
}
foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        $call.logical_pc -notin $allowedPcs -or
        $call.input_x -ne $expectedSlots[$call.logical_pc] -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Giant-Face call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Giant-Face call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "7428" = 1
    "742B" = 1
    "742E" = 1
    "74D8" = 2648
    "766C" = 1
    "767E" = 873
    "76F7" = 1
    "76FA" = 867
    "7718" = 1
    "7726" = 1214
} "Giant-Face routine"

Assert-Histogram @($calls | Where-Object logical_pc -eq "74D8") "return_instruction_pc" @{
    "74DF" = 2648
} "Giant-Face controller wait return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "767E") "return_instruction_pc" @{
    "7685" = 873
} "Giant-Face animator wait return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "76FA") "return_instruction_pc" @{
    "7717" = 867
} "Giant-Face crown no-hit return"
Assert-Histogram @($calls | Where-Object logical_pc -eq "7726") "return_instruction_pc" @{
    "7748" = 1214
} "Giant-Face mouth no-hit return"

$expectedZeroWriteCounts = @{
    "74D8" = 2625
    "767E" = 865
    "76FA" = 860
    "7726" = 1192
}
foreach ($pc in $expectedZeroWriteCounts.Keys) {
    $rows = @($calls | Where-Object logical_pc -eq $pc)
    if (
        @($rows | Where-Object write_count -eq "0").Count -ne
            $expectedZeroWriteCounts[$pc]
    ) {
        throw "Giant-Face $pc zero-write population changed."
    }
}

# Only initializer/restoration writes are semantically owned by bank 03 in
# this route. All later scoped writes are interrupt stack/driver activity.
$directState = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            [Convert]::ToInt32($_.work_ram_offset, 16) -ge 0x0200
        } |
        ForEach-Object {
            "$($_.call_sequence):$($_.instruction_logical_pc):" +
                "$($_.work_ram_offset):$($_.value)"
        }
)
$expectedDirectState = @(
    "2:766E:1E37:01",
    "2:7673:1BB7:08",
    "2:7678:1A37:01",
    "3:771D:1838:00",
    "3:7720:19B8:00",
    "6:74C6:0258:00",
    "6:74C9:0D0D:00",
    "6:74CC:0D0C:00",
    "6:74CF:0D0E:00",
    "6:74D2:1E36:00"
)
if (($directState -join ",") -ne ($expectedDirectState -join ",")) {
    throw "Giant-Face routed direct-write behavior changed."
}

$layoutTypes = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4131" -and
            $_.logical_address -in @("3036", "3037", "3038", "3039")
        } |
        ForEach-Object { "$($_.logical_address):$($_.value)" }
)
if (($layoutTypes -join ",") -ne "3036:83,3037:83,3038:82,3039:82") {
    throw "Giant-Face fixed type identities changed."
}

$pointerTimeline = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -ge 871 -and
            $_.logical_address -in @(
                "3F36", "3FB6", "3F37", "3FB7",
                "3F38", "3FB8", "3F39", "3FB9"
            ) -and
            $_.instruction_logical_pc -in @("C5E1", "C5E6")
        } |
        ForEach-Object {
            "$($_.frame):$($_.logical_address):$($_.value)"
        }
)
$expectedPointerTimeline = @(
    "872:3F36:2B",
    "872:3FB6:74",
    "872:3F37:7E",
    "872:3FB7:76",
    "872:3F38:26",
    "872:3FB8:77",
    "872:3F39:FA",
    "872:3FB9:76",
    "873:3F36:2E",
    "873:3FB6:74",
    "941:3F36:D8",
    "941:3FB6:74"
)
if (
    ($pointerTimeline -join ",") -ne
        ($expectedPointerTimeline -join ",")
) {
    throw "Giant-Face dispatch timeline changed."
}

$controllerReset = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -ge 871 -and
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -in @(
                "74C6", "74C9", "74CC", "74CF", "74D2"
            )
        } |
        ForEach-Object {
            "$($_.frame):$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedControllerReset = @(
    "941:74C6:2258:00",
    "941:74C9:2D0D:00",
    "941:74CC:2D0C:00",
    "941:74CF:2D0E:00",
    "941:74D2:3E36:00"
)
if (
    ($controllerReset -join ",") -ne
        ($expectedControllerReset -join ",")
) {
    throw "Giant-Face inactive restoration changed."
}

# No wake/progression request, crown/mouth hit, open-mouth award, audio
# command, type-$84 transition, or bonus entrance occurs in this route.
$inactivePcs = @(
    "74E0", "7503", "7508", "752B", "7532", "7590", "7595",
    "7701", "770E", "7732", "773F", "7749", "7755", "7768",
    "776D", "779F"
)
$inactiveWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "03" -and
            $_.instruction_logical_pc -in $inactivePcs
        }
)
if ($inactiveWrites.Count -ne 0) {
    throw "Giant-Face inactive transformation/hit path became active."
}

$controller = @($calls | Where-Object logical_pc -eq "74D8")
$animator = @($calls | Where-Object logical_pc -eq "767E")
$crown = @($calls | Where-Object logical_pc -eq "76FA")
$mouth = @($calls | Where-Object logical_pc -eq "7726")
Assert-Call $calls[0] 1 872 "7428" "36" "C5E9" 192 7 (
    "Giant-Face controller first stage"
)
Assert-Call $controller[0] 7 942 "74D8" "36" "74DF" 27 0 (
    "Giant-Face controller normalized wait sample"
)
Assert-Call $animator[0] 297 1222 "767E" "37" "7685" 27 0 (
    "Giant-Face animator normalized wait sample"
)
Assert-Call $crown[0] 302 1223 "76FA" "39" "7717" 27 0 (
    "Giant-Face crown normalized no-hit sample"
)
Assert-Call $mouth[0] 279 1213 "7726" "38" "7748" 48 0 (
    "Giant-Face mouth normalized no-hit sample"
)

Write-Output "Giant-Face semantics verified."
Write-Output (
    "Fixed slots 36-39 map the form controller, hidden animator, mouth, " +
    "and crown collision surface."
)
Write-Output (
    "5608 routed calls retain inactive form 0; request flags, hits, " +
    "awards, audio, and bonus entrance remain inactive."
)
Write-Output (
    "ROM-bounded rule: request bits wake and advance six transformations; " +
    "form 6 opens the mouth."
)
Write-Output (
    "ROM-bounded collisions: crown/closed mouth award 3 with command 20; " +
    "open mouth awards 100, emits command 41, and selects bonus stage 4."
)
Write-Output (
    "Normalized samples: 03:74D8@1, 03:767E@1, 03:76FA@1, and " +
    "03:7726@1."
)
