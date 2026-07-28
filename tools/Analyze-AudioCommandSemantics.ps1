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
    [string] $ActorWritePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-RomBytes {
    param(
        [byte[]] $Rom,
        [int] $Offset,
        [byte[]] $Expected,
        [string] $Label
    )

    if ($Offset -lt 0 -or $Offset + $Expected.Count -gt $Rom.Count) {
        throw "$Label is outside the ROM."
    }
    for ($index = 0; $index -lt $Expected.Count; ++$index) {
        if ($Rom[$Offset + $index] -ne $Expected[$index]) {
            throw (
                "$Label changed at ROM offset 0x{0:X6}: expected {1:X2}, " +
                "found {2:X2}."
            ) -f (
                $Offset + $index
            ), $Expected[$index], $Rom[$Offset + $index]
        }
    }
}

function Import-TraceCsv {
    param(
        [string] $Path
    )

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

function Get-LastValueBeforeCycle {
    param(
        [hashtable] $WritesByAddress,
        [int] $Address,
        [int64] $Cycle
    )

    $key = "{0:X4}" -f $Address
    if (-not $WritesByAddress.ContainsKey($key)) {
        return "--"
    }
    $priorWrites = @(
        $WritesByAddress[$key] |
            Where-Object {
                [int64] $_.absolute_master_cycle -lt $Cycle
            }
    )
    if ($priorWrites.Count -eq 0) {
        return "--"
    }
    return $priorWrites[-1].value
}

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# C087 is the bank-1 JMP-table entry used by mapped gameplay code. ECFD
# captures A as the command at $2A and filters commands while audio is locked.
# Accepted commands fall through to ED18, which selects export 6 in bank $45.
Assert-RomBytes $rom 0x002087 @(
    0x4C, 0xFD, 0xEC
) "C087 audio-command entry"
Assert-RomBytes $rom 0x000CFD @(
    0x85, 0x2A, 0xC9, 0xFF, 0xD0, 0x01, 0x60,
    0xAD, 0xC7, 0x2C, 0xF0, 0x0F,
    0xA5, 0x2A, 0xC9, 0x31, 0xF0, 0x09,
    0xC9, 0x27, 0xF0, 0x05,
    0xC9, 0x47, 0xF0, 0x01, 0x60
) "ECFD command latch and filter"
Assert-RomBytes $rom 0x000D18 @(
    0xA9, 0x06, 0x85, 0x2F,
    0xA9, 0x45, 0x85, 0x30,
    0x4C, 0x58, 0xED
) "ED18 audio export selector"

# ED58 maps raw banks $45-$47 into MPR2-MPR4, calls the common indirect
# trampoline, and restores every prior mapping before returning.
Assert-RomBytes $rom 0x000D58 @(
    0xDA,
    0xA5, 0x1E, 0x48, 0xA5, 0x1F, 0x48, 0xA5, 0x20, 0x48,
    0xA5, 0x30, 0x85, 0x1E, 0x53, 0x04,
    0x1A, 0x85, 0x1F, 0x53, 0x08,
    0x1A, 0x85, 0x20, 0x53, 0x10,
    0x20, 0x62, 0xE2,
    0x68, 0x85, 0x20, 0x53, 0x10,
    0x68, 0x85, 0x1F, 0x53, 0x08,
    0x68, 0x85, 0x1E, 0x53, 0x04,
    0xFA, 0x60
) "ED58 audio bank wrapper"
Assert-RomBytes $rom 0x000262 @(
    0xA5, 0x2F, 0x8D, 0xBE, 0x2C,
    0xA9, 0x40, 0x8D, 0xBF, 0x2C,
    0xA5, 0x2A, 0x6C, 0xBE, 0x2C
) "E262 banked-call trampoline"

# Raw mapper bank $45 mirrors backing ROM bank $25 in this cartridge image.
Assert-RomBytes $rom 0x04A006 @(
    0x4C, 0x7B, 0x4B
) "4006 audio-command export"
Assert-RomBytes $rom 0x04AB7B @(
    0x08, 0xDA, 0x5A, 0x78, 0x20, 0xCC, 0x4B,
    0x7A, 0xFA, 0x28, 0x60
) "4B7B interrupt-safe command wrapper"
Assert-RomBytes $rom 0x04ABCC @(
    0x85, 0xD4, 0x0A, 0xAA,
    0xBD, 0x81, 0x5D, 0x85, 0xD2,
    0xBD, 0x82, 0x5D, 0x85, 0xD3,
    0xA0, 0x00, 0xB1, 0xD2, 0xD0, 0x62
) "4BCC command descriptor lookup"

# These fixed records own every layout-backed gameplay audio producer on the
# route. Keeping their initializer, position, state, flags, type, and
# parameter bytes together avoids inventing names before visual correlation.
$expectedAudioActorRecords = @(
    @(0x0E, @(0x00, 0x0E, 0xEE, 0x50, 0x08, 0x01, 0x76, 0x02,
        0x05, 0x00, 0x82, 0x00)),
    @(0x13, @(0x00, 0x13, 0x31, 0x5A, 0x40, 0x00, 0xFE, 0x01,
        0x0C, 0x8A, 0x82, 0x00)),
    @(0x14, @(0x02, 0x14, 0x9A, 0x78, 0xBE, 0x00, 0x5C, 0x02,
        0x0E, 0x03, 0x82, 0x00)),
    @(0x15, @(0x02, 0x15, 0xA4, 0x78, 0x4A, 0x00, 0x5C, 0x02,
        0x0F, 0x03, 0x82, 0x01)),
    @(0x26, @(0x00, 0x26, 0x6B, 0x64, 0x40, 0x00, 0x7C, 0x02,
        0x29, 0x82, 0x83, 0x00)),
    @(0x27, @(0x00, 0x27, 0x6B, 0x64, 0xD8, 0x00, 0x7C, 0x02,
        0x29, 0x82, 0x83, 0x01)),
    @(0x2D, @(0x00, 0x2D, 0xB3, 0x60, 0x90, 0x00, 0x1A, 0x02,
        0x18, 0xA0, 0x83, 0x00)),
    @(0x30, @(0x00, 0x30, 0xE4, 0x5E, 0xE0, 0x00, 0x06, 0x02,
        0x1F, 0x82, 0x83, 0x00)),
    @(0x59, @(0x02, 0x59, 0x4A, 0x66, 0xA0, 0x00, 0xFE, 0x00,
        0x41, 0x86, 0x82, 0x01)),
    @(0x5A, @(0x02, 0x5A, 0x4A, 0x66, 0xC0, 0x00, 0xFE, 0x00,
        0x41, 0x86, 0x82, 0x02))
)
foreach ($entry in $expectedAudioActorRecords) {
    $slot = [int] $entry[0]
    Assert-RomBytes $rom (0x0045DB + 12 * ($slot - 0x0A)) (
        [byte[]] $entry[1]
    ) ("Audio actor record {0:X2}" -f $slot)
}

$calls = @(Import-TraceCsv $CallPath)
$writes = @(Import-TraceCsv $WritePath)
if ($calls.Count -ne 66 -or $writes.Count -ne 7261) {
    throw (
        "Expected 66 accepted calls and 7,261 scoped writes; found " +
        "$($calls.Count) calls and $($writes.Count) writes."
    )
}

for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    if (
        [int] $call.sequence -ne $index + 1 -or
        $call.physical_bank -ne "00" -or
        $call.logical_pc -ne "ED18" -or
        $call.return_physical_bank -ne "00" -or
        $call.return_instruction_pc -ne "ED85"
    ) {
        throw "ED18 bounded-call identity changed at sequence $($index + 1)."
    }
    if (
        $call.input_x -ne $call.output_x -or
        $call.input_y -ne $call.output_y -or
        $call.input_sp -ne $call.output_sp
    ) {
        throw "ED18 stopped preserving X, Y, or SP at sequence $($index + 1)."
    }
    foreach ($mpr in 0..7) {
        if ($call."input_mpr$mpr" -ne $call."output_mpr$mpr") {
            throw "ED18 stopped restoring MPR$mpr at sequence $($index + 1)."
        }
    }
}

$firstCall = $calls[0]
$bodyCycles = @($calls | ForEach-Object { [int64] $_.body_master_cycles })
$writeCounts = @($calls | ForEach-Object { [int] $_.write_count })
if (
    [int] $firstCall.frame -ne 7 -or
    [int64] $firstCall.body_master_cycles -ne 2400 -or
    [int] $firstCall.write_count -ne 66 -or
    ($bodyCycles | Measure-Object -Minimum).Minimum -ne 2400 -or
    ($bodyCycles | Measure-Object -Maximum).Maximum -ne 10791 -or
    ($writeCounts | Measure-Object -Minimum).Minimum -ne 66 -or
    ($writeCounts | Measure-Object -Maximum).Maximum -ne 271
) {
    throw "ED18 bounded timing or write-count range changed."
}

$commandWrites = @(
    $writes |
        Where-Object {
            $_.instruction_physical_bank -eq "45" -and
            $_.instruction_logical_pc -eq "4BCC" -and
            $_.work_ram_offset -eq "00D4"
        }
)
if ($commandWrites.Count -ne 66) {
    throw "Expected one 4BCC command latch per accepted ED18 call."
}
for ($index = 0; $index -lt $commandWrites.Count; ++$index) {
    if ([int] $commandWrites[$index].call_sequence -ne $index + 1) {
        throw "Command latch order changed at sequence $($index + 1)."
    }
}

$expectedCommands = @{
    "01" = 1
    "0C" = 2
    "20" = 11
    "21" = 5
    "25" = 1
    "29" = 7
    "2B" = 1
    "2D" = 5
    "2E" = 6
    "33" = 1
    "34" = 4
    "39" = 1
    "3D" = 4
    "3E" = 1
    "42" = 1
    "44" = 15
}
Assert-Histogram $commandWrites "value" $expectedCommands "Command ID"

$producerRows = for ($index = 0; $index -lt $calls.Count; ++$index) {
    [pscustomobject] @{
        Producer = (
            $calls[$index].caller_pc + "/" + $calls[$index].return_pc +
            ":" + $commandWrites[$index].value
        )
    }
}
$expectedProducers = @{
    "42A3/42A6:2D" = 5
    "42A3/42A6:34" = 1
    "42A3/42A6:39" = 1
    "518A/518D:3D" = 4
    "5202/5205:2B" = 1
    "52B3/52B6:25" = 1
    "55FC/55FF:3E" = 1
    "5633/5636:0C" = 1
    "5638/563B:42" = 1
    "570F/5712:2E" = 1
    "573F/5742:2E" = 1
    "5777/577A:2E" = 1
    "5A60/5A63:2E" = 2
    "5F1B/5F1E:2E" = 1
    "6033/6036:29" = 7
    "60DD/60E0:33" = 1
    "64B4/64B7:34" = 3
    "67CC/67CF:44" = 15
    "7091/7094:0C" = 1
    "78F2/78F5:20" = 9
    "790F/7912:20" = 2
    "7F75/7F78:21" = 5
    "C13E/C141:01" = 1
}
Assert-Histogram $producerRows "Producer" $expectedProducers "Producer/command"

if ($commandWrites[0].value -ne "3E") {
    throw "The first normalized command sample changed from ID 3E."
}

# The independent actor identity trace comes from the same deterministic
# route. Aligning absolute cycles recovers each slot's occupancy/type and
# live $3F80:$3F00 dispatch pointer immediately before the audio call.
$actorWrites = @(Import-TraceCsv $ActorWritePath)
if ($actorWrites.Count -ne 3872) {
    throw "Expected 3,872 actor identity writes."
}
$writesByAddress = $actorWrites |
    Group-Object -Property logical_address -AsHashTable -AsString
$actorContexts = for ($index = 0; $index -lt $calls.Count; ++$index) {
    $call = $calls[$index]
    $slot = [Convert]::ToInt32($call.input_x, 16)
    $cycle = [int64] $call.entry_absolute_master_cycle
    $type = Get-LastValueBeforeCycle $writesByAddress (
        0x3000 + $slot
    ) $cycle
    $dispatchLow = Get-LastValueBeforeCycle $writesByAddress (
        0x3F00 + $slot
    ) $cycle
    $dispatchHigh = Get-LastValueBeforeCycle $writesByAddress (
        0x3F80 + $slot
    ) $cycle
    $dispatch = if (
        $dispatchLow -ne "--" -and $dispatchHigh -ne "--"
    ) {
        $dispatchHigh + $dispatchLow
    } else {
        "----"
    }
    [pscustomobject] @{
        Context = (
            "{0}:{1:X2}:{2}:{3}" -f
                $commandWrites[$index].value,
                $slot,
                $type,
                $dispatch
        )
    }
}
$expectedActorContexts = @{
    "01:00:80:C696" = 1
    "0C:05:80:7084" = 1
    "0C:06:80:71CD" = 1
    "20:14:82:78C7" = 3
    "20:15:82:78C7" = 8
    "21:62:82:7F57" = 2
    "21:63:82:7F57" = 1
    "21:69:82:7F57" = 2
    "25:0E:82:5116" = 1
    "29:64:82:6016" = 2
    "29:67:82:6016" = 1
    "29:6A:82:6016" = 2
    "29:6D:82:6016" = 2
    "2B:0E:82:5159" = 1
    "2D:62:8F:4290" = 2
    "2D:69:8F:4290" = 1
    "2D:70:8F:4290" = 1
    "2D:71:8F:4290" = 1
    "2E:10:00:0000" = 3
    "2E:13:82:5A4B" = 2
    "2E:30:83:5EEC" = 1
    "33:2D:83:60C3" = 1
    "34:26:83:6490" = 1
    "34:27:83:6490" = 2
    "34:69:8F:4290" = 1
    "39:62:8F:4290" = 1
    "3D:0E:82:5159" = 4
    "3E:06:80:715A" = 1
    "42:06:80:71CD" = 1
    "44:59:82:66A3" = 6
    "44:59:82:66D3" = 1
    "44:5A:82:66A3" = 1
    "44:5A:82:66D3" = 7
}
Assert-Histogram (
    $actorContexts
) "Context" $expectedActorContexts "Audio actor context"

# The doubled command ID indexes little-endian descriptor pointers at $5D81.
# Observed descriptors live in the $45 or $47 mapped windows and all take the
# data-descriptor path (a zero tag would be interpreted as an executable tail).
$expectedDescriptors = @(
    @("01", 0x5E11, 0x06),
    @("0C", 0x89C0, 0x06),
    @("20", 0x9103, 0x02),
    @("21", 0x9128, 0x02),
    @("25", 0x9183, 0x01),
    @("29", 0x8C07, 0x02),
    @("2B", 0x8C70, 0x03),
    @("2D", 0x8CC2, 0x02),
    @("2E", 0x8CF6, 0x02),
    @("33", 0x8DCC, 0x01),
    @("34", 0x8DE7, 0x01),
    @("39", 0x8EC5, 0x02),
    @("3D", 0x8F4C, 0x02),
    @("3E", 0x8F99, 0x01),
    @("42", 0x9056, 0x01),
    @("44", 0x908A, 0x01)
)
foreach ($descriptor in $expectedDescriptors) {
    $id = [Convert]::ToInt32($descriptor[0], 16)
    $pointerOffset = 0x04BD81 + 2 * $id
    $pointer = $rom[$pointerOffset] + 256 * $rom[$pointerOffset + 1]
    if ($pointer -ne $descriptor[1]) {
        throw "Command $($descriptor[0]) descriptor pointer changed."
    }

    if ($pointer -ge 0x4000 -and $pointer -le 0x5FFF) {
        $descriptorOffset = 0x04A000 + $pointer - 0x4000
    } elseif ($pointer -ge 0x8000 -and $pointer -le 0x9FFF) {
        $descriptorOffset = 0x04E000 + $pointer - 0x8000
    } else {
        throw "Command $($descriptor[0]) descriptor left mapped audio ROM."
    }
    if (
        $rom[$descriptorOffset] -eq 0 -or
        $rom[$descriptorOffset] -ne $descriptor[2]
    ) {
        throw "Command $($descriptor[0]) descriptor tag changed."
    }
}

$driverWrites = @(
    $writes |
        Where-Object { $_.instruction_physical_bank -eq "45" }
)
$bankZeroWrites = @(
    $writes |
        Where-Object { $_.instruction_physical_bank -eq "00" }
)
$channelStateWrites = @(
    $writes |
        Where-Object {
            $offset = [Convert]::ToInt32($_.work_ram_offset, 16)
            $offset -ge 0x0E00 -and $offset -le 0x0FFF
        }
)
if (
    $driverWrites.Count -ne 5628 -or
    $bankZeroWrites.Count -ne 1633 -or
    $channelStateWrites.Count -ne 2737 -or
    @(
        $writes |
            Where-Object {
                $_.instruction_physical_bank -ne "45" -and
                $_.instruction_physical_bank -ne "00"
            }
    ).Count -ne 0 -or
    @(
        $channelStateWrites |
            Where-Object { $_.instruction_physical_bank -ne "45" }
    ).Count -ne 0
) {
    throw "Audio driver or channel-state write ownership changed."
}

Write-Output "Audio command semantics verified."
Write-Output (
    "ED18: 66 accepted commands across 21 caller paths; " +
    "body budgets are 2400-10791 master cycles."
)
Write-Output (
    "Command latch: 16 IDs pass from `$2A to 45:4BCC/`$00D4."
)
Write-Output (
    "Driver descriptors: 16 nonzero tags produce 2737 writes in " +
    "`$2E00-`$2FFF."
)
Write-Output (
    "Gameplay ownership: ball slot 00, 10 layout records, and 3 dynamic " +
    "dispatch families."
)
Write-Output (
    "First normalized sample: frame 7, command 3E, 2400 cycles, 66 writes."
)
