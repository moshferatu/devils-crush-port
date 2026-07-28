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

# The four type-$84 records are the lower-left, lower-right, upper-left, and
# upper-right Screen-3 Stars. The manual calls these Wall Pockets.
Assert-RomHex $rom 0x00484B @"
00 3E FC 49 50 00 D6 00 16 A2 84 00
00 3F 0E 4A FE 00 D6 00 16 A2 84 00
"@ "Lower Four-Star Wall-Pocket layout records"
Assert-RomHex $rom 0x00487B @"
00 42 20 4A 56 00 4C 00 16 82 84 00
00 43 32 4A F6 00 4C 00 16 82 84 00
"@ "Upper Four-Star Wall-Pocket layout records"

# Each initializer assigns a ball-routing value at $3C80,X, a bonus-stage ID
# at $3700,X, and a zero-based Roman-numeral selector at $3780,X:
# lower-left BS-1/VI, lower-right BS-5/IV, upper-left BS-3/VIII, and
# upper-right BS-6/II.
Assert-RomHex $rom 0x0049FC @"
A9 1C 9D 80 3C A9 01 9D 00 37 A9 05 9D 80 37 4C
6C 4A A9 14 9D 80 3C A9 05 9D 00 37 A9 03 9D 80
37 4C 6C 4A A9 03 9D 80 3C A9 03 9D 00 37 A9 07
9D 80 37 4C 6C 4A A9 0A 9D 80 3C A9 06 9D 00 37
A9 01 9D 80 37 4C 6C 4A
"@ "Four-Star Wall-Pocket initializers"

# Shared setup installs the four-tick idle cycle and 02:4A93. A collision
# checks that Big-Star angular velocity $2D00:$2D01 is zero and that selected
# direction $2D18 matches this pocket's Roman selector. A mismatch runs the
# normal path and advances Sorcerer color $2D40 modulo eight. A match emits
# command $41, awards 100 stored units, and passes the per-pocket stage ID to
# the shared scene-transition helper.
Assert-RomHex $rom 0x004A6C @"
BD 80 37 C9 03 F0 07 C9 05 F0 03 9C 33 2D A9 01
9D 00 3A A9 6C 9D 00 3E A9 4A 9D 80 3E A9 93 9D
00 3F A9 4A 9D 80 3F BD 00 38 30 03 4C DA 4C BD
80 37 C9 03 F0 09 C9 05 F0 05 A9 01 8D 33 2D AD
00 2D 0D 01 2D D0 08 AD 18 2D DD 80 37 F0 0F 20
69 C0 AD 40 2D 1A 29 07 8D 40 2D 4C F2 4C 9C FA
2C A9 41 20 87 C0 A9 64 85 2F 64 30 20 2A C0 A9
04 9D 00 3A 20 6F 40 A9 04 8D 80 39 20 18 C0 AD
80 39 C9 06 F0 0E DE 00 3A D0 08 A9 04 9D 00 3A
EE 80 39 60 AD 80 30 09 20 8D 80 30 BD 00 37 85
55 A5 53 09 10 85 53 20 18 C0 A9 06 8D 80 39 AD
80 3C 09 01 8D 80 3C 4C 20 4D
"@ "Four-Star Wall-Pocket setup and access decision"

# The normal branch awards 10 stored units. Both access branches converge on
# the shared opening animation, command $25, ball routing through $3C80,X,
# and restoration of the original dispatch.
Assert-RomHex $rom 0x004CF2 @"
A9 0A 85 2F 64 30 20 2A C0 A9 04 9D 00 3A 20 6F
40 A9 04 8D 80 39 20 18 C0 AD 80 39 C9 06 F0 0E
DE 00 3A D0 08 A9 04 9D 00 3A EE 80 39 60 AD 80
30 09 20 8D 80 30 A9 60 20 21 C0 BD 00 3E C9 78
D0 1F BD 80 3E C9 4C D0 18 BD 80 3D 29 07 D0 11
A9 20 20 87 C0 20 69 78 A9 03 85 2F 64 30 20 2A
C0 20 18 C0 AD 80 30 29 DF 8D 80 30 A9 04 9D 00
3A A9 25 20 87 C0 20 18 C0 AD 80 39 C9 03 F0 0E
DE 00 3A D0 08 A9 04 9D 00 3A CE 80 39 60 DA 02
A2 00 A5 83 9D 80 38 B9 80 3C 20 75 C0 20 1E C0
AD 80 3C 29 FE 8D 80 3C FA BD 80 30 09 40 9D 80
30 A9 08 20 21 C0 20 18 C0 BD 80 30 29 BF 9D 80
30 BD 00 38 29 7F 9D 00 38 BD 00 3E 9D 00 3F BD
80 3E 9D 80 3F
"@ "Four-Star Wall-Pocket normal award and ball-return path"

# The neighboring Big-Star controller initializes an unselected direction and
# zero rotation, then copies the stopped arrow position into $2D18.
Assert-RomHex $rom 0x00565C @"
A9 01 8D 04 2D A9 80 8D 18 2D 9C 00 2D 9C 01 2D
"@ "Big-Star selector initialization"
Assert-RomHex $rom 0x0056F6 @"
A9 0C 85 37 BD 80 39 8D 18 2D
"@ "Big-Star stopped-arrow selection"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 1016 -or
    $writes.Count -ne 2036 -or
    $stateWrites.Count -ne 3951
) {
    throw (
        "Expected 1,016 calls, 2,036 scoped writes, and 3,951 selected " +
        "Four-Star Wall-Pocket state writes."
    )
}

$allowedPcs = @("49FC", "4A0E", "4A20", "4A32", "4A6C", "4A93")
foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $validSlot = switch ($call.logical_pc) {
        "49FC" { $call.input_x -eq "3E" }
        "4A0E" { $call.input_x -eq "3F" }
        "4A20" { $call.input_x -eq "42" }
        "4A32" { $call.input_x -eq "43" }
        default { $call.input_x -in @("3E", "3F", "42", "43") }
    }
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "02" -or
        $call.logical_pc -notin $allowedPcs -or
        -not $validSlot -or
        $call.return_instruction_pc -ne "4CF1" -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw (
            "Four-Star Wall-Pocket call identity changed at sequence " +
            "$($index + 1)."
        )
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw (
                "Four-Star Wall-Pocket call changed MPR$mpr at sequence " +
                "$($index + 1)."
            )
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "49FC" = 1
    "4A0E" = 1
    "4A20" = 1
    "4A32" = 1
    "4A6C" = 4
    "4A93" = 1008
} "Four-Star Wall-Pocket routine"

$steady = @($calls | Where-Object logical_pc -eq "4A93")
Assert-Histogram $steady "input_x" @{
    "3E" = 503
    "3F" = 503
    "42" = 1
    "43" = 1
} "Four-Star Wall-Pocket steady slot"
if (
    ($steady | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Minimum).Minimum -ne 66 -or
    ($steady | ForEach-Object {
        [int64] $_.body_master_cycles
    } | Measure-Object -Maximum).Maximum -ne 1098 -or
    ($steady | ForEach-Object {
        [int] $_.write_count
    } | Measure-Object -Minimum).Minimum -ne 1 -or
    ($steady | ForEach-Object {
        [int] $_.write_count
    } | Measure-Object -Maximum).Maximum -ne 19
) {
    throw "Four-Star Wall-Pocket steady timing/write range changed."
}

# The four records retain type $84, route values, stage IDs, Roman selectors,
# timer 1, original-state marker 4A6C, and live dispatch 4A93.
$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in @(
                "4125", "4131",
                "49FE", "4A03", "4A08",
                "4A10", "4A15", "4A1A",
                "4A22", "4A27", "4A2C",
                "4A34", "4A39", "4A3E",
                "4A77", "4A7C", "4A81", "4A86", "4A8B", "4A90"
            )
        } |
        ForEach-Object {
            "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @(
    "4125:393E:16",
    "4131:303E:84",
    "49FE:3CBE:1C",
    "4A03:373E:01",
    "4A08:37BE:05",
    "4A7C:3A3E:01",
    "4A81:3E3E:6C",
    "4A86:3EBE:4A",
    "4A8B:3F3E:93",
    "4A90:3FBE:4A",
    "4125:393F:16",
    "4131:303F:84",
    "4A10:3CBF:14",
    "4A15:373F:05",
    "4A1A:37BF:03",
    "4A7C:3A3F:01",
    "4A81:3E3F:6C",
    "4A86:3EBF:4A",
    "4A8B:3F3F:93",
    "4A90:3FBF:4A",
    "4125:3942:16",
    "4131:3042:84",
    "4A22:3CC2:03",
    "4A27:3742:03",
    "4A2C:37C2:07",
    "4A77:2D33:00",
    "4A7C:3A42:01",
    "4A81:3E42:6C",
    "4A86:3EC2:4A",
    "4A8B:3F42:93",
    "4A90:3FC2:4A",
    "4125:3943:16",
    "4131:3043:84",
    "4A34:3CC3:0A",
    "4A39:3743:06",
    "4A3E:37C3:01",
    "4A77:2D33:00",
    "4A7C:3A43:01",
    "4A81:3E43:6C",
    "4A86:3EC3:4A",
    "4A8B:3F43:93",
    "4A90:3FC3:4A"
)
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Four-Star Wall-Pocket initial identity changed."
}

# All routed calls take the same four-tick idle cycle. Only the lower pair
# becomes camera-active after initialization; the upper pair stays dormant.
Assert-Histogram @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in @("4CDA", "4CE1", "4CE4", "4CEE")
        }
) "instruction_logical_pc" @{
    "4CDA" = 1008
    "4CE1" = 254
    "4CE4" = 254
    "4CEE" = 84
} "Four-Star Wall-Pocket idle animation write"

$timerDecrements = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4CDA"
        }
)
Assert-Histogram $timerDecrements "logical_address" @{
    "3A3E" = 503
    "3A3F" = 503
    "3A42" = 1
    "3A43" = 1
} "Four-Star Wall-Pocket timer slot"
Assert-Histogram $timerDecrements "value" @{
    "00" = 254
    "01" = 250
    "02" = 252
    "03" = 252
} "Four-Star Wall-Pocket timer value"

$appearanceAdvances = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4CE4"
        }
)
Assert-Histogram $appearanceAdvances "logical_address" @{
    "39BE" = 126
    "39BF" = 126
    "39C2" = 1
    "39C3" = 1
} "Four-Star Wall-Pocket appearance slot"
Assert-Histogram $appearanceAdvances "value" @{
    "01" = 86
    "02" = 84
    "03" = 84
} "Four-Star Wall-Pocket appearance value"

$appearanceResets = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -eq "4CEE"
        }
)
Assert-Histogram $appearanceResets "logical_address" @{
    "39BE" = 42
    "39BF" = 42
} "Four-Star Wall-Pocket appearance reset slot"
if (@($appearanceResets | Where-Object value -ne "00").Count -ne 0) {
    throw "Four-Star Wall-Pocket appearance reset value changed."
}

# No pocket is entered on this route. Normal access would write $2D40 at
# 4AC4; selected bonus access would clear $2CFA at 4ACA.
$activationWrites = @(
    $stateWrites |
        Where-Object {
            $_.instruction_physical_bank -eq "02" -and
            $_.instruction_logical_pc -in @("4AA8", "4AC4", "4ACA")
        }
)
if ($activationWrites.Count -ne 0) {
    throw "Four-Star Wall-Pocket access path became active."
}

Assert-Call $calls[0] 1 872 "49FC" "3E" "4CF1" 360 11 (
    "Lower-left Four-Star Wall-Pocket initializer"
)
Assert-Call $calls[3] 4 872 "4A0E" "3F" "4CF1" 348 11 (
    "Lower-right Four-Star Wall-Pocket initializer"
)
Assert-Call $calls[6] 7 872 "4A20" "42" "4CF1" 369 12 (
    "Upper-left Four-Star Wall-Pocket initializer"
)
Assert-Call $calls[9] 10 872 "4A32" "43" "4CF1" 369 12 (
    "Upper-right Four-Star Wall-Pocket initializer"
)
Assert-Call $steady[0] 3 872 "4A93" "3E" "4CF1" 135 3 (
    "Four-Star Wall-Pocket normalized idle sample"
)

Write-Output "Four-Star Wall-Pocket semantics verified."
Write-Output (
    "Slots 3E/3F/42/43 map lower-left/right and upper-left/right " +
    "Wall Pockets to BS-1/5/3/6 and selectors VI/IV/VIII/II."
)
Write-Output (
    "1008 idle calls leave all four access paths inactive; only the lower " +
    "pair becomes camera-active after initialization."
)
Write-Output (
    "ROM-bounded normal access awards 10 and cycles Sorcerer color; a " +
    "stopped matching Big-Star arrow awards 100 and selects the pocket BS."
)
Write-Output (
    "Normalized sample: 02:4A93@1, frame 872, 135 cycles, 3 writes."
)
