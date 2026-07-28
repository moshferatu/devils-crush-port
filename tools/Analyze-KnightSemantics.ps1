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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# Four category-1 fixed records form the vertical Knight queue immediately
# above the Giant Door. Their positions and four independent initializer
# entries retain queue indices zero through three.
Assert-RomHex $rom 0x00481B @"
01 3A 4E 73 20 00 C6 00 09 00 82 00
01 3B 53 73 20 00 B6 00 09 00 82 00
01 3C 5A 73 20 00 A6 00 09 00 82 00
01 3D 61 73 20 00 96 00 09 00 82 00
"@ "Knight layout records"

# The four entries assign indices 0-3, then share completion selector $8B,
# collision selector $10, four-hit counter 4, and steady dispatch 03:7378.
Assert-RomHex $rom 0x00734E @"
9E 00 3B 80 13 A9 01 9D 00 3B 80 0C A9 02 9D 00 3B
80 05 A9 03 9D 00 3B A9 8B 9D 00 3D A9 10 9D 00 38
A9 04 9D 00 3C 20 18 C0
"@ "Knight initialization"

# Steady state waits on bit 0 of its indexed Giant-Door handshake byte. Once
# released it clears that bit, enters the movement state, and retains the
# indexed four-hit collision helper.
Assert-RomHex $rom 0x007378 @"
BC 00 3B B9 06 2D 29 01 D0 06 20 D8 73 B0 77 60 BC
00 3B B9 06 2D 29 FE 99 06 2D A9 80 9D 80 38 A9 04
9D 00 3A 9E 00 3E 9E 80 3E 20 18 C0 BC 00 3B B9 24
74 20 66 69 B0 1C DE 00 3A D0 12 A9 04 9D 00 3A FE
80 39 BD 80 39 C9 04 90 03 9E 80 39 44 0E B0 32 60
20 18 C0 BC 00 3B 44 03 B0 27 60
"@ "Knight wait and movement states"

# Nonterminal hits award 50 stored units (5,000 displayed points) and command
# $26. The fourth hit takes the carry return, awards 100 stored units (the
# normal 5,000-point hit plus 5,000-point destruction bonus), acknowledges
# bit 1 to the matching Giant-Door channel, and releases the actor surface.
Assert-RomHex $rom 0x0073D8 @"
BD 00 38 10 1F 29 7F 9D 00 38 DE 00 3C D0 02 38 60
20 F6 7B A9 26 20 87 C0 A9 32 85 2F A9 00 85 30 20
2A C0 18 60 A9 64 85 2F 64 30 20 2A C0 20 63 C0 BC
00 3B B9 06 2D 09 02 99 06 2D 20 A0 41 BD 80 30 09
60 9D 80 30 20 18 C0 60 01 02 03 04
"@ "Knight hit and completion states"

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
$stateWrites = @(Import-TraceCsv $StateWritePath)
if (
    $calls.Count -ne 10604 -or
    $writes.Count -ne 27576 -or
    $stateWrites.Count -ne 2264
) {
    throw (
        "Expected 10,604 calls, 27,576 scoped writes, and 2,264 selected " +
        "Knight state writes."
    )
}

$initializerBySlot = @{
    "3A" = "734E"
    "3B" = "7353"
    "3C" = "735A"
    "3D" = "7361"
}
foreach ($index in 0..($calls.Count - 1)) {
    $call = $calls[$index]
    $isInitializer = $call.logical_pc -ne "7378"
    $validInitializer = (
        $initializerBySlot.ContainsKey($call.input_x) -and
        $initializerBySlot[$call.input_x] -eq $call.logical_pc
    )
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "03" -or
        (
            $isInitializer -and
            (
                -not $validInitializer -or
                $call.return_instruction_pc -ne "C5E9"
            )
        ) -or
        (
            -not $isInitializer -and
            (
                $call.input_x -notin @("3A", "3B", "3C", "3D") -or
                $call.return_instruction_pc -ne "7387"
            )
        ) -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "Knight call identity changed at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "Knight call changed MPR$mpr at sequence $($index + 1)."
        }
    }
}

Assert-Histogram $calls "logical_pc" @{
    "734E" = 1
    "7353" = 1
    "735A" = 1
    "7361" = 1
    "7378" = 10600
} "Knight routine"

$steady = @($calls | Where-Object logical_pc -eq "7378")
Assert-Histogram $steady "input_x" @{
    "3A" = 2650
    "3B" = 2650
    "3C" = 2650
    "3D" = 2650
} "Knight steady slot"
if (
    [int] $steady[0].frame -ne 873 -or
    [int] $steady[-1].frame -ne 3600
) {
    throw "Knight steady frame bounds changed."
}

$leafCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -eq 123 -and
            [int] $_.write_count -eq 2
        }
)
if ($leafCalls.Count -ne 10129) {
    throw "Knight zero-activation leaf-call population changed."
}
$interruptCalls = @(
    $steady |
        Where-Object {
            [int64] $_.body_master_cycles -ne 123 -or
            [int] $_.write_count -ne 2
        }
)
if (
    $interruptCalls.Count -ne 471 -or
    ($interruptCalls | Measure-Object body_master_cycles -Minimum).Minimum -ne
        645 -or
    ($interruptCalls | Measure-Object body_master_cycles -Maximum).Maximum -ne
        1950 -or
    ($interruptCalls | Measure-Object write_count -Minimum).Minimum -ne 12 -or
    ($interruptCalls | Measure-Object write_count -Maximum).Maximum -ne 34
) {
    throw "Knight interrupt-bearing call population changed."
}

# Frame 872 fixes the four indices, shared selectors/counters, actor identity,
# and common steady pointer.
$initialState = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -eq 872 -and
            $_.instruction_logical_pc -in @(
                "4125", "412B", "4131",
                "734E", "7355", "735C", "7363",
                "7368", "736D", "7372", "C5E1", "C5E6"
            )
        } |
        ForEach-Object {
            "$($_.instruction_physical_bank):" +
                "$($_.instruction_logical_pc):" +
                "$($_.logical_address):$($_.value)"
        }
)
$expectedInitialState = @()
foreach ($slot in @("3A", "3B", "3C", "3D")) {
    $index = [Convert]::ToInt32($slot, 16) - 0x3A
    $indexPc = @("734E", "7355", "735C", "7363")[$index]
    $expectedInitialState += @(
        "02:4125:39${slot}:09",
        "02:412B:30$('{0:X2}' -f (0xBA + $index)):00",
        "02:4131:30${slot}:82",
        "03:${indexPc}:3B${slot}:$('{0:X2}' -f $index)",
        "03:7368:3D${slot}:8B",
        "03:736D:38${slot}:10",
        "03:7372:3C${slot}:04",
        "01:C5E1:3F${slot}:78",
        "01:C5E6:3F$('{0:X2}' -f (0xBA + $index)):73"
    )
}
if (($initialState -join ",") -ne ($expectedInitialState -join ",")) {
    throw "Knight initial identity changed."
}

# The four actors move progressively farther above the route's camera window.
# No gate-release bit or actor-owned state changes after initialization.
$cameraExpected = @{
    "30BA" = @{"F520" = 437; "F54B" = 82}
    "30BB" = @{"F520" = 448; "F54B" = 71}
    "30BC" = @{"F520" = 459; "F54B" = 60}
    "30BD" = @{"F520" = 480; "F54B" = 39}
}
foreach ($address in $cameraExpected.Keys) {
    Assert-Histogram @(
        $stateWrites |
            Where-Object {
                $_.logical_address -eq $address -and
                $_.instruction_logical_pc -in @("F520", "F54B")
            }
    ) "instruction_logical_pc" $cameraExpected[$address] (
        "Knight $address camera-window write"
    )
}

$postInitializationWrites = @(
    $stateWrites |
        Where-Object {
            [int] $_.frame -gt 872 -and
            $_.logical_address -notin @("30BA", "30BB", "30BC", "30BD")
        }
)
if ($postInitializationWrites.Count -ne 0) {
    throw "The routed Knight release/hit path became active."
}

if (
    [int64] $steady[0].body_master_cycles -ne 123 -or
    [int] $steady[0].write_count -ne 2
) {
    throw "Knight normalized wait sample changed."
}

Write-Output "Knight semantics verified."
Write-Output (
    "Fixed slots 3A-3D form the four-Knight queue above the Giant Door " +
    "and index handshake bytes 2D06-2D09."
)
Write-Output (
    "10600 steady calls remain gate-blocked: 10129 are 123-cycle " +
    "two-stack-write leaves and 471 contain periodic interrupts."
)
Write-Output (
    "ROM-bounded Knights take four hits, use command 26 and 50-unit " +
    "awards, then add 100 units and acknowledge their indexed channel."
)
Write-Output (
    "Normalized sample: 03:7378@1, frame 873, 123 cycles, 2 writes."
)
