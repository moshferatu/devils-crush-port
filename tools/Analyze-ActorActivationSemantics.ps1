# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [Parameter(Mandatory = $true)]
    [string] $CallPath,

    [Parameter(Mandatory = $true)]
    [string] $WritePath
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

$rom = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath)
)
if ($rom.Count -ne 393216) {
    throw "Expected the 393,216-byte supported ROM."
}

# F4DF sign-extends the camera velocity into scratch storage, starts at slot
# zero, and calls F4FC once for every index through 0x7E.
Assert-RomBytes $rom 0x0014DF @(
    0x38, 0x62, 0xE5, 0xA6, 0x85, 0x35,
    0x62, 0xE5, 0xA7, 0x85, 0x36,
    0x64, 0x26, 0xA5, 0x36, 0x10, 0x02, 0xC6, 0x26,
    0x82, 0x20, 0xFC, 0xF4, 0xE8, 0xE0, 0x7F, 0xD0, 0xF8,
    0x60
) "F4DF 127-slot sweep"

# An empty $3000,X slot returns without mutation. Occupied slots compare their
# signed integer Y coordinate at $3380:$3300,X against the half-open interval
# [$AA:$A9 - 0x30, $AA:$A9 + 0xF0). Bit 7 of $3080,X records membership.
Assert-RomBytes $rom 0x0014FC @(
    0xBD, 0x00, 0x30, 0xF0, 0x4D,
    0x38, 0xA5, 0xA9, 0xE9, 0x30, 0x85, 0x35,
    0xA5, 0xAA, 0xE9, 0x00, 0x85, 0x36,
    0x38, 0xBD, 0x00, 0x33, 0xE5, 0x35,
    0xBD, 0x80, 0x33, 0xE5, 0x36, 0x10, 0x09,
    0xBD, 0x80, 0x30, 0x29, 0x7F, 0x9D, 0x80, 0x30, 0x60,
    0x18, 0xA5, 0xA9, 0x69, 0xF0, 0x85, 0x35,
    0x62, 0x65, 0xAA, 0x85, 0x36,
    0x38, 0xBD, 0x00, 0x33, 0xE5, 0x35,
    0xBD, 0x80, 0x33, 0xE5, 0x36, 0x30, 0x09,
    0xBD, 0x80, 0x30, 0x29, 0x7F, 0x9D, 0x80, 0x30, 0x60,
    0xBD, 0x80, 0x30, 0x09, 0x80, 0x9D, 0x80, 0x30, 0x60
) "F4FC camera-window flag update"

# Newly initialized mechanisms can refresh one slot through the fixed-bank
# trampoline instead of waiting for the next complete sweep.
Assert-RomBytes $rom 0x00206C @(
    0x4C, 0xFC, 0xF4
) "C06C F4FC trampoline"
Assert-RomBytes $rom 0x005FD3 @(
    0x20, 0x6C, 0xC0
) "First active direct-refresh caller"
Assert-RomBytes $rom 0x007EEB @(
    0x20, 0x6C, 0xC0
) "Second active direct-refresh caller"

$calls = Import-TraceCsv $CallPath
if ($calls.Count -ne 65927) {
    throw "Expected 65,927 F4FC calls, found $($calls.Count)."
}

$expectedCallerCounts = @{
    "F4F3:F4F6" = 65913
    "5FD3:5FD6" = 4
    "7EEB:7EEE" = 10
}
$actualCallerCounts = @{}
$pathCounts = @{
    "above" = 0
    "below" = 0
    "empty" = 0
    "inside" = 0
}
$pathBySequence = @{}
$occupiedBySlot = @{}
$emptyBySlot = @{}
$mainCalls = [System.Collections.Generic.List[object]]::new()
$directCalls = [System.Collections.Generic.List[object]]::new()

foreach ($call in $calls) {
    if (
        $call.physical_bank -ne "00" -or
        $call.logical_pc -ne "F4FC" -or
        $call.return_physical_bank -ne "00"
    ) {
        throw "Actor-window call $($call.sequence) changed routine identity."
    }

    $slot = [Convert]::ToInt32($call.input_x, 16)
    if ($slot -lt 0 -or $slot -gt 0x7E) {
        throw "Actor-window call $($call.sequence) used invalid slot $slot."
    }

    $callerKey = "$($call.caller_pc):$($call.return_pc)"
    if (-not $actualCallerCounts.ContainsKey($callerKey)) {
        $actualCallerCounts[$callerKey] = 0
    }
    ++$actualCallerCounts[$callerKey]

    if ($callerKey -eq "F4F3:F4F6") {
        $mainCalls.Add($call)
    } else {
        $directCalls.Add($call)
    }

    $outputA = [Convert]::ToInt32($call.output_a, 16)
    if ($call.return_instruction_pc -eq "F523") {
        $path = "below"
    } elseif ($call.return_instruction_pc -eq "F545") {
        $path = "above"
    } elseif (
        $call.return_instruction_pc -eq "F54E" -and
        ($outputA -band 0x80) -ne 0
    ) {
        $path = "inside"
    } elseif (
        $call.return_instruction_pc -eq "F54E" -and
        $outputA -eq 0
    ) {
        $path = "empty"
    } else {
        throw "Actor-window call $($call.sequence) changed its bounded path."
    }

    ++$pathCounts[$path]
    $pathBySequence[[string] $call.sequence] = $path
    if ($path -eq "empty") {
        if (-not $emptyBySlot.ContainsKey($slot)) {
            $emptyBySlot[$slot] = 0
        }
        ++$emptyBySlot[$slot]
    } else {
        if (-not $occupiedBySlot.ContainsKey($slot)) {
            $occupiedBySlot[$slot] = 0
        }
        ++$occupiedBySlot[$slot]
    }
}

if ($actualCallerCounts.Count -ne $expectedCallerCounts.Count) {
    throw "F4FC caller set changed."
}
foreach ($key in $expectedCallerCounts.Keys) {
    if (
        -not $actualCallerCounts.ContainsKey($key) -or
        $actualCallerCounts[$key] -ne $expectedCallerCounts[$key]
    ) {
        throw "F4FC caller count changed for $key."
    }
}

$expectedPathCounts = @{
    "above" = 8525
    "below" = 28823
    "empty" = 10873
    "inside" = 17706
}
foreach ($path in $expectedPathCounts.Keys) {
    if ($pathCounts[$path] -ne $expectedPathCounts[$path]) {
        throw "F4FC $path path count changed."
    }
}

# Removing the 14 direct refreshes must leave 519 complete, ordered sweeps.
if ($mainCalls.Count -ne 519 * 127) {
    throw "The route no longer contains 519 complete F4DF sweeps."
}
for ($sweep = 0; $sweep -lt 519; ++$sweep) {
    for ($slot = 0; $slot -lt 127; ++$slot) {
        $call = $mainCalls[$sweep * 127 + $slot]
        if ([Convert]::ToInt32($call.input_x, 16) -ne $slot) {
            throw "F4DF sweep $($sweep + 1) changed slot order at $slot."
        }
    }
}
if (
    [int] $mainCalls[0].frame -ne 872 -or
    [int] $mainCalls[$mainCalls.Count - 1].frame -ne 3600
) {
    throw "F4DF sweep frame bounds changed."
}

$expectedDirectSlots = 0x62..0x6F
$expectedDirectFrames = @(
    873, 873, 941, 988, 988, 989, 1036,
    1036, 1037, 1084, 1084, 1085, 1132, 1132
)
for ($index = 0; $index -lt $directCalls.Count; ++$index) {
    if (
        [Convert]::ToInt32($directCalls[$index].input_x, 16) -ne
            $expectedDirectSlots[$index] -or
        [int] $directCalls[$index].frame -ne $expectedDirectFrames[$index]
    ) {
        throw "Direct actor-slot refresh $($index + 1) changed."
    }
}

# The route keeps the main table mechanisms in slots 04-61 allocated,
# dynamically allocates 62-71, and never allocates the reserved edge ranges.
$expectedAlwaysEmpty = @(
    0x01, 0x02, 0x03,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E
)
$actualAlwaysEmpty = @(
    0..0x7E |
        Where-Object { -not $occupiedBySlot.ContainsKey($_) }
)
if (
    ($actualAlwaysEmpty -join ",") -ne
    ($expectedAlwaysEmpty -join ",")
) {
    throw "The always-empty actor-slot set changed."
}
if ($occupiedBySlot.Count -ne 111) {
    throw "Expected 111 slots to become occupied."
}
foreach ($slot in 0x04..0x61) {
    if ($emptyBySlot.ContainsKey($slot)) {
        throw ("Persistent actor slot 0x{0:X2} became empty." -f $slot)
    }
}
foreach ($slot in 0x62..0x71) {
    if (
        -not $occupiedBySlot.ContainsKey($slot) -or
        -not $emptyBySlot.ContainsKey($slot)
    ) {
        throw ("Dynamic actor slot 0x{0:X2} lost a lifecycle state." -f $slot)
    }
}

# Scoped traces include nested interrupt writes. Match the instruction, slot,
# and returned value so an interrupt between the final store and RTS does not
# hide the routine's own flag write.
$finalWrites = @{}
foreach ($write in (Import-TraceCsv $WritePath)) {
    if (
        $write.instruction_physical_bank -ne "00" -or
        $write.instruction_logical_pc -notin @("F520", "F542", "F54B")
    ) {
        continue
    }
    $sequence = [string] $write.call_sequence
    $call = $calls[[int] $sequence - 1]
    $path = $pathBySequence[$sequence]
    if ($path -eq "empty") {
        continue
    }
    $expectedPc = if ($path -eq "below") {
        "F520"
    } elseif ($path -eq "above") {
        "F542"
    } else {
        "F54B"
    }
    $expectedOffset = 0x1080 +
        [Convert]::ToInt32($call.input_x, 16)
    if (
        $write.instruction_logical_pc -eq $expectedPc -and
        [Convert]::ToInt32($write.work_ram_offset, 16) -eq
            $expectedOffset -and
        [Convert]::ToInt32($write.value, 16) -eq
            [Convert]::ToInt32($call.output_a, 16)
    ) {
        $finalWrites[$sequence] = $write
    }
}

if ($finalWrites.Count -ne 55054) {
    throw (
        "Expected 55,054 occupied-slot flag writes, found " +
        "$($finalWrites.Count)."
    )
}
foreach ($call in $calls) {
    $sequence = [string] $call.sequence
    $path = $pathBySequence[$sequence]
    if ($path -eq "empty") {
        continue
    }

    if (-not $finalWrites.ContainsKey($sequence)) {
        throw "Occupied slot call $sequence omitted its final flag write."
    }
    $write = $finalWrites[$sequence]
    $value = [Convert]::ToInt32($write.value, 16)
    $outputA = [Convert]::ToInt32($call.output_a, 16)
    if ($value -ne $outputA) {
        throw "Call $sequence did not return its stored flag value."
    }
    if ($path -eq "inside") {
        if (
            $write.instruction_logical_pc -ne "F54B" -or
            ($value -band 0x80) -eq 0
        ) {
            throw "Call $sequence did not set the in-window flag."
        }
    } else {
        $expectedPc = if ($path -eq "below") { "F520" } else { "F542" }
        if (
            $write.instruction_logical_pc -ne $expectedPc -or
            ($value -band 0x80) -ne 0
        ) {
            throw "Call $sequence did not clear the out-of-window flag."
        }
    }
}

$first = $calls[0]
if (
    [int] $first.frame -ne 872 -or
    [int] $first.body_master_cycles -ne 333 -or
    [int] $first.write_count -ne 5 -or
    $pathBySequence["1"] -ne "inside"
) {
    throw "The normalized first actor-window sample changed."
}

Write-Output "Actor activation semantics verified."
Write-Output (
    "F4DF: 519 complete sweeps cover slots 00-7E; " +
    "14 direct refreshes cover slots 62-6F."
)
Write-Output (
    "F4FC paths: empty=10873, below=28823, inside=17706, above=8525."
)
Write-Output (
    "Actor slots: 111 occupied; 04-61 persistent; 62-71 dynamic."
)
