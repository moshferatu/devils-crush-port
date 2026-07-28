# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BeetleTracePath,

    [Parameter(Mandatory = $true)]
    [string] $MesenTracePath,

    [Parameter(Mandatory = $true)]
    [string] $BeetleCallPath,

    [Parameter(Mandatory = $true)]
    [string] $BeetleWritePath,

    [Parameter(Mandatory = $true)]
    [string] $MesenCallPath,

    [Parameter(Mandatory = $true)]
    [string] $MesenWritePath,

    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [ValidateSet("gameplay", "physics")]
    [string] $Profile = "gameplay"
)

$ErrorActionPreference = "Stop"
$script:exactBoundedCalls = 0
$script:interruptRelocatedCalls = 0
$script:interruptInternalWrites = 0
$script:physicsExactLeafCalls = 0
$script:physicsStateDivergentLeafCalls = 0

function Import-BeetleTrace {
    param([Parameter(Mandatory = $true)][string] $Path)

    $lines = Get-Content -LiteralPath $Path
    $headerIndex = [Array]::FindIndex(
        [string[]] $lines,
        [Predicate[string]] {
            param($line)
            return $line.StartsWith("sequence,frame,")
        }
    )
    if ($headerIndex -lt 0) {
        throw "The Beetle trace has no CSV header."
    }
    return @($lines[$headerIndex..($lines.Count - 1)] | ConvertFrom-Csv)
}

function Import-GeneratedCsv {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $HeaderPrefix
    )

    $lines = Get-Content -LiteralPath $Path
    $headerIndex = [Array]::FindIndex(
        [string[]] $lines,
        [Predicate[string]] {
            param($line)
            return $line.StartsWith($HeaderPrefix)
        }
    )
    if ($headerIndex -lt 0) {
        throw "$Path has no '$HeaderPrefix' CSV header."
    }
    return @($lines[$headerIndex..($lines.Count - 1)] | ConvertFrom-Csv)
}

function Assert-Count {
    param(
        [Parameter(Mandatory = $true)][object[]] $Rows,
        [Parameter(Mandatory = $true)][string] $Bank,
        [Parameter(Mandatory = $true)][string] $Pc,
        [Parameter(Mandatory = $true)][int] $Expected,
        [Parameter(Mandatory = $true)][string] $Oracle
    )

    $actual = @(
        $Rows | Where-Object {
            $_.physical_bank -eq $Bank -and $_.logical_pc -eq $Pc
        }
    ).Count
    if ($actual -ne $Expected) {
        throw (
            "$Oracle $Bank`:$Pc count changed: expected $Expected, " +
            "actual $actual."
        )
    }
}

function Get-RoutineRows {
    param(
        [Parameter(Mandatory = $true)][object[]] $Rows,
        [Parameter(Mandatory = $true)][string] $Bank,
        [Parameter(Mandatory = $true)][string] $Pc
    )

    return @(
        $Rows | Where-Object {
            $_.physical_bank -eq $Bank -and $_.logical_pc -eq $Pc
        }
    )
}

function Convert-HexByte {
    param([Parameter(Mandatory = $true)][string] $Value)
    return [Convert]::ToInt32($Value, 16)
}

function Convert-HexWord {
    param([Parameter(Mandatory = $true)][string] $Value)
    return [Convert]::ToInt32($Value, 16)
}

$opcodeSizes = [byte[]] @(
    2,2,1,2,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,2,2,2,2,2,1,3,1,1,3,3,3,3,
    3,2,1,2,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,1,2,2,2,2,1,3,1,1,3,3,3,3,
    1,2,1,2,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,2,1,2,2,2,1,3,1,1,1,3,3,3,
    1,2,1,1,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,7,2,2,2,2,1,3,1,1,3,3,3,3,
    2,2,1,3,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,4,2,2,2,2,1,3,1,1,3,3,3,3,
    2,2,2,3,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,4,2,2,2,2,1,3,1,1,3,3,3,3,
    2,2,1,7,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,7,1,2,2,2,1,3,1,1,1,3,3,3,
    2,2,1,7,2,2,2,2,1,2,1,1,3,3,3,3,
    2,2,2,7,1,2,2,2,1,3,1,1,1,3,3,3
)
$romBytes = [System.IO.File]::ReadAllBytes(
    (Resolve-Path -LiteralPath $RomPath).Path
)

function Assert-AlignedRows {
    param(
        [Parameter(Mandatory = $true)][object[]] $BeetleRows,
        [Parameter(Mandatory = $true)][object[]] $MesenRows,
        [Parameter(Mandatory = $true)][int] $MesenStartIndex,
        [Parameter(Mandatory = $true)][int] $FrameOffset,
        [Parameter(Mandatory = $true)][string] $Routine,
        [int[]] $AllowedFrameOffsets = @($FrameOffset)
    )

    if ($BeetleRows.Count -ne $MesenRows.Count - $MesenStartIndex) {
        throw "$Routine aligned call counts differ."
    }

    $stableFields = @(
        "a",
        "x",
        "y",
        "sp",
        "mpr0",
        "mpr1",
        "mpr2",
        "mpr3",
        "mpr6",
        "mpr7"
    )
    for ($index = 0; $index -lt $BeetleRows.Count; $index++) {
        $beetle = $BeetleRows[$index]
        $mesen = $MesenRows[$index + $MesenStartIndex]
        $actualFrameOffset = [int] $beetle.frame - [int] $mesen.frame
        if ($AllowedFrameOffsets -notcontains $actualFrameOffset) {
            throw "$Routine frame alignment changed at call $($index + 1)."
        }
        foreach ($field in $stableFields) {
            if ($beetle.$field -ne $mesen.$field) {
                throw (
                    "$Routine $field changed at aligned call " +
                    "$($index + 1)."
                )
            }
        }

        $beetleStatus = Convert-HexByte $beetle.status
        $mesenStatus = Convert-HexByte $mesen.status
        if (($beetleStatus -band 0xEF) -ne ($mesenStatus -band 0xEF)) {
            throw (
                "$Routine status differs beyond the transient T flag at " +
                "aligned call $($index + 1)."
            )
        }
    }
}

function Group-WritesByCall {
    param([Parameter(Mandatory = $true)][object[]] $Rows)

    $result = @{}
    foreach ($group in ($Rows | Group-Object call_sequence)) {
        $result[$group.Name] = @($group.Group)
    }
    return $result
}

function Get-SemanticWriteView {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [object[]] $Rows,
        [string] $OwningPhysicalBank = ""
    )

    $excluded = [bool[]]::new($Rows.Count)
    $hasInterrupt = $false
    for ($index = 0; $index -lt $Rows.Count; $index++) {
        $bank = Convert-HexByte $Rows[$index].instruction_physical_bank
        $pc = Convert-HexWord $Rows[$index].instruction_logical_pc
        if ($bank -eq 0 -and $pc -ge 0xFC00) {
            $excluded[$index] = $true
            $hasInterrupt = $true
        }
    }
    for ($index = 0; $index + 3 -lt $Rows.Count; $index++) {
        $offset0 = Convert-HexWord $Rows[$index].work_ram_offset
        $offset1 = Convert-HexWord $Rows[$index + 1].work_ram_offset
        $offset2 = Convert-HexWord $Rows[$index + 2].work_ram_offset
        $afterBank =
            Convert-HexByte $Rows[$index + 3].instruction_physical_bank
        if ($offset0 -ge 0x100 -and $offset0 -le 0x1FF -and
            $offset1 -eq $offset0 - 1 -and
            $offset2 -eq $offset1 - 1 -and
            $afterBank -eq 0) {
            $excluded[$index] = $true
            $excluded[$index + 1] = $true
            $excluded[$index + 2] = $true
            $handlerIndex = $index + 3
            while ($handlerIndex -lt $Rows.Count) {
                $handlerBank = Convert-HexByte (
                    $Rows[$handlerIndex].instruction_physical_bank
                )
                $handlerPc = Convert-HexWord (
                    $Rows[$handlerIndex].instruction_logical_pc
                )
                $returnedToBankZeroOwner =
                    $OwningPhysicalBank -eq "00" -and $handlerPc -lt 0xFC00
                if ($handlerBank -ne 0 -or $returnedToBankZeroOwner) {
                    break
                }
                $excluded[$handlerIndex] = $true
                ++$handlerIndex
            }
            $hasInterrupt = $true
        }
    }

    $semantic = @()
    for ($index = 0; $index -lt $Rows.Count; $index++) {
        if (!$excluded[$index]) {
            $semantic += $Rows[$index]
        }
    }
    return [pscustomobject] @{
        Rows = $semantic
        HasInterrupt = $hasInterrupt
    }
}

function Assert-AlignedCalls {
    param(
        [Parameter(Mandatory = $true)][object[]] $BeetleRows,
        [Parameter(Mandatory = $true)][object[]] $MesenRows,
        [Parameter(Mandatory = $true)][hashtable] $BeetleWrites,
        [Parameter(Mandatory = $true)][hashtable] $MesenWrites,
        [Parameter(Mandatory = $true)][int] $FrameOffset,
        [Parameter(Mandatory = $true)][string] $Routine,
        [string[]] $IgnoredFields = @(),
        [int[]] $AllowedFrameOffsets = @($FrameOffset)
    )

    if ($BeetleRows.Count -ne $MesenRows.Count) {
        throw "$Routine aligned bounded-call counts differ."
    }

    $stableFields = @(
        "caller_pc",
        "return_pc",
        "return_physical_bank",
        "return_instruction_pc",
        "input_a",
        "input_x",
        "input_y",
        "input_sp",
        "input_mpr0",
        "input_mpr1",
        "input_mpr2",
        "input_mpr3",
        "input_mpr6",
        "input_mpr7",
        "output_a",
        "output_x",
        "output_y",
        "output_sp",
        "output_mpr0",
        "output_mpr1",
        "output_mpr2",
        "output_mpr3",
        "output_mpr6",
        "output_mpr7"
    )
    $writeFields = @(
        "instruction_physical_bank",
        "work_ram_offset"
    )

    for ($index = 0; $index -lt $BeetleRows.Count; $index++) {
        $beetle = $BeetleRows[$index]
        $mesen = $MesenRows[$index]
        $actualFrameOffset = [int] $beetle.frame - [int] $mesen.frame
        if ($AllowedFrameOffsets -notcontains $actualFrameOffset) {
            throw (
                "$Routine bounded-call frame alignment changed at call " +
                "$($index + 1)."
            )
        }
        foreach ($field in $stableFields) {
            if ($IgnoredFields -contains $field) {
                continue
            }
            if ($beetle.$field -ne $mesen.$field) {
                throw (
                    "$Routine bounded-call $field changed at aligned call " +
                    "$($index + 1): Beetle=$($beetle.$field), " +
                    "Mesen=$($mesen.$field)."
                )
            }
        }
        foreach ($field in @("input_status", "output_status")) {
            $beetleStatusMissing =
                [string]::IsNullOrEmpty($beetle.$field)
            $mesenStatusMissing =
                [string]::IsNullOrEmpty($mesen.$field)
            if ($beetleStatusMissing -or $mesenStatusMissing) {
                throw (
                    "$Routine bounded-call $field is missing at aligned " +
                    "call $($index + 1)."
                )
            }
            $beetleStatus = Convert-HexByte $beetle.$field
            $mesenStatus = Convert-HexByte $mesen.$field
            if (($beetleStatus -band 0xEF) -ne ($mesenStatus -band 0xEF)) {
                throw (
                    "$Routine bounded-call $field differs beyond T at " +
                    "aligned call $($index + 1)."
                )
            }
        }

        $beetleCallWrites = @()
        if ($BeetleWrites.ContainsKey($beetle.sequence)) {
            $beetleCallWrites = @($BeetleWrites[$beetle.sequence])
        }
        $mesenCallWrites = @()
        if ($MesenWrites.ContainsKey($mesen.sequence)) {
            $mesenCallWrites = @($MesenWrites[$mesen.sequence])
        }
        $interruptRelocated =
            $beetleCallWrites.Count -ne $mesenCallWrites.Count
        if ($interruptRelocated) {
            $cycleDelta =
                [int64] $mesen.body_master_cycles -
                [int64] $beetle.body_master_cycles
            $writeDelta =
                $mesenCallWrites.Count - $beetleCallWrites.Count
            $cycleDeltaMagnitude = [Math]::Abs($cycleDelta)
            $cycleDeltaSign = [Math]::Sign($cycleDelta)
            $writeDeltaSign = [Math]::Sign($writeDelta)
            $unexplainedRelocation = (
                $cycleDeltaMagnitude -lt 3 -or
                $cycleDeltaSign -ne $writeDeltaSign
            )
            if ($unexplainedRelocation) {
                throw (
                    "$Routine has an unexplained scoped timing/write " +
                    "difference at aligned call $($index + 1)."
                )
            }
            ++$script:interruptRelocatedCalls
        } elseif (
            $beetle.body_master_cycles -ne $mesen.body_master_cycles) {
            throw (
                "$Routine cycle budget changed without an interrupt-write " +
                "relocation at aligned call $($index + 1)."
            )
        } else {
            ++$script:exactBoundedCalls
        }
        $beetleWriteView = Get-SemanticWriteView `
            $beetleCallWrites `
            $beetle.physical_bank
        $mesenWriteView = Get-SemanticWriteView `
            $mesenCallWrites `
            $mesen.physical_bank
        $beetleCallWrites = @($beetleWriteView.Rows)
        $mesenCallWrites = @($mesenWriteView.Rows)
        if ($beetleCallWrites.Count -ne $mesenCallWrites.Count) {
            throw (
                "$Routine semantic ordered-write count changed at aligned " +
                "call $($index + 1)."
            )
        }
        $scopeContainsInterrupt =
            $interruptRelocated -or
            $beetleWriteView.HasInterrupt -or
            $mesenWriteView.HasInterrupt
        for ($writeIndex = 0;
             $writeIndex -lt $beetleCallWrites.Count;
             $writeIndex++) {
            $beetleWrite = $beetleCallWrites[$writeIndex]
            $mesenWrite = $mesenCallWrites[$writeIndex]
            foreach ($field in $writeFields) {
                if ($beetleWrite.$field -ne $mesenWrite.$field) {
                    throw (
                        "$Routine ordered-write $field changed at call " +
                        "$($index + 1), write $($writeIndex + 1): " +
                        "Beetle=$($beetleWrite.$field), " +
                        "Mesen=$($mesenWrite.$field)."
                    )
                }
            }
            $writerBank = Convert-HexByte (
                $beetleWrite.instruction_physical_bank
            )
            $beetleWriterMissing = [string]::IsNullOrEmpty(
                $beetleWrite.instruction_logical_pc
            )
            $mesenWriterMissing = [string]::IsNullOrEmpty(
                $mesenWrite.instruction_logical_pc
            )
            if ($beetleWriterMissing -or $mesenWriterMissing) {
                throw (
                    "$Routine writer PC is missing at call " +
                    "$($index + 1), write $($writeIndex + 1)."
                )
            }
            $writerPc = Convert-HexWord (
                $beetleWrite.instruction_logical_pc
            )
            $romOffset =
                $writerBank * 0x2000 + ($writerPc -band 0x1FFF)
            if ($romOffset -lt 0 -or $romOffset -ge $romBytes.Length) {
                throw "$Routine writer PC lies outside the supported ROM."
            }
            $expectedMesenPc =
                ($writerPc + $opcodeSizes[$romBytes[$romOffset]]) -band 0xFFFF
            $actualMesenPc = Convert-HexWord (
                $mesenWrite.instruction_logical_pc
            )
            if ($actualMesenPc -ne $writerPc -and
                $actualMesenPc -ne $expectedMesenPc) {
                throw (
                    "$Routine writer instruction changed at call " +
                    "$($index + 1), write $($writeIndex + 1): " +
                    "Beetle=$($beetleWrite.instruction_logical_pc), " +
                    "Mesen-next=$($mesenWrite.instruction_logical_pc)."
                )
            }
            $interruptInternalWrite = $actualMesenPc -eq $writerPc
            if ($interruptInternalWrite) {
                ++$script:interruptInternalWrites
            }
            if ($beetleWrite.value -ne $mesenWrite.value) {
                throw (
                    "$Routine ordered-write value changed at call " +
                    "$($index + 1), write $($writeIndex + 1): " +
                    "Beetle=$($beetleWrite.value), " +
                    "Mesen=$($mesenWrite.value)."
                )
            }
            $beetleOffset =
                [uint64] $beetleWrite.absolute_master_cycle -
                [uint64] $beetle.entry_absolute_master_cycle
            $mesenOffset =
                [uint64] $mesenWrite.absolute_master_cycle -
                [uint64] $mesen.entry_absolute_master_cycle
            if (!$interruptInternalWrite -and !$scopeContainsInterrupt -and
                $beetleOffset -ne $mesenOffset) {
                throw (
                    "$Routine ordered-write cycle changed at call " +
                    "$($index + 1), write $($writeIndex + 1): " +
                    "Beetle=$beetleOffset, Mesen=$mesenOffset."
                )
            }
        }
    }
}

function Compare-PhysicsLeafCalls {
    param(
        [Parameter(Mandatory = $true)][object[]] $BeetleRows,
        [Parameter(Mandatory = $true)][object[]] $MesenRows,
        [Parameter(Mandatory = $true)][hashtable] $BeetleWrites,
        [Parameter(Mandatory = $true)][hashtable] $MesenWrites,
        [Parameter(Mandatory = $true)][string] $Routine
    )

    if ($BeetleRows.Count -ne $MesenRows.Count) {
        throw "$Routine leaf-call counts differ."
    }

    $requiredFields = @(
        "caller_pc",
        "return_pc",
        "return_physical_bank",
        "return_instruction_pc",
        "input_a",
        "input_x",
        "input_y",
        "input_sp",
        "input_mpr0",
        "input_mpr1",
        "input_mpr2",
        "input_mpr3",
        "input_mpr6",
        "input_mpr7"
    )
    $outputFields = @(
        "output_a",
        "output_x",
        "output_y",
        "output_sp",
        "output_status",
        "output_mpr0",
        "output_mpr1",
        "output_mpr2",
        "output_mpr3",
        "output_mpr6",
        "output_mpr7"
    )

    for ($index = 0; $index -lt $BeetleRows.Count; $index++) {
        $beetle = $BeetleRows[$index]
        $mesen = $MesenRows[$index]
        foreach ($field in $requiredFields) {
            if ($beetle.$field -ne $mesen.$field) {
                throw (
                    "$Routine required $field changed at leaf call " +
                    "$($index + 1)."
                )
            }
        }
        if (((Convert-HexByte $beetle.input_status) -band 0xEF) -ne
            ((Convert-HexByte $mesen.input_status) -band 0xEF)) {
            throw "$Routine input status differs beyond T at leaf call."
        }

        $semanticMatch = $true
        foreach ($field in $outputFields) {
            if ($beetle.$field -ne $mesen.$field) {
                $semanticMatch = $false
                break
            }
        }

        $beetleCallWrites = @()
        if ($BeetleWrites.ContainsKey($beetle.sequence)) {
            $beetleCallWrites = @($BeetleWrites[$beetle.sequence])
        }
        $mesenCallWrites = @()
        if ($MesenWrites.ContainsKey($mesen.sequence)) {
            $mesenCallWrites = @($MesenWrites[$mesen.sequence])
        }
        $beetleView = Get-SemanticWriteView `
            $beetleCallWrites `
            $beetle.physical_bank
        $mesenView = Get-SemanticWriteView `
            $mesenCallWrites `
            $mesen.physical_bank
        $beetleSemanticWrites = @($beetleView.Rows)
        $mesenSemanticWrites = @($mesenView.Rows)
        if ($beetleSemanticWrites.Count -ne $mesenSemanticWrites.Count) {
            $semanticMatch = $false
        } else {
            for ($writeIndex = 0;
                 $writeIndex -lt $beetleSemanticWrites.Count;
                 $writeIndex++) {
                $beetleWrite = $beetleSemanticWrites[$writeIndex]
                $mesenWrite = $mesenSemanticWrites[$writeIndex]
                foreach ($field in @(
                    "instruction_physical_bank",
                    "work_ram_offset",
                    "value"
                )) {
                    if ($beetleWrite.$field -ne $mesenWrite.$field) {
                        $semanticMatch = $false
                        break
                    }
                }
                if (!$semanticMatch) {
                    break
                }
            }
        }

        if ($semanticMatch) {
            ++$script:physicsExactLeafCalls
        } else {
            ++$script:physicsStateDivergentLeafCalls
        }
    }
}

$beetle = Import-BeetleTrace (
    (Resolve-Path -LiteralPath $BeetleTracePath).Path
)
$mesen = @(
    Import-Csv (Resolve-Path -LiteralPath $MesenTracePath).Path
)
$beetleCalls = Import-GeneratedCsv `
    (Resolve-Path -LiteralPath $BeetleCallPath).Path `
    "sequence,frame,"
$beetleWriteRows = Import-GeneratedCsv `
    (Resolve-Path -LiteralPath $BeetleWritePath).Path `
    "call_sequence,write_sequence,"
$mesenCalls = @(
    Import-Csv (Resolve-Path -LiteralPath $MesenCallPath).Path
)
$mesenWriteRows = @(
    Import-Csv (Resolve-Path -LiteralPath $MesenWritePath).Path
)
$beetleWrites = Group-WritesByCall $beetleWriteRows
$mesenWrites = Group-WritesByCall $mesenWriteRows

if ($Profile -eq "physics") {
    $physicsRoutines = @(
        [pscustomobject] @{
            Bank = "00"; Pc = "F1B4"; BeetleCount = 1156; MesenCount = 1157
        },
        [pscustomobject] @{
            Bank = "00"; Pc = "F241"; BeetleCount = 404; MesenCount = 404
        },
        [pscustomobject] @{
            Bank = "01"; Pc = "D1A2"; BeetleCount = 404; MesenCount = 404
        },
        [pscustomobject] @{
            Bank = "01"; Pc = "DD09"; BeetleCount = 753; MesenCount = 754
        }
    )

    foreach ($routine in $physicsRoutines) {
        $name = "$($routine.Bank):$($routine.Pc)"
        Assert-Count `
            $beetle `
            $routine.Bank `
            $routine.Pc `
            $routine.BeetleCount `
            "Beetle"
        Assert-Count `
            $mesen `
            $routine.Bank `
            $routine.Pc `
            $routine.MesenCount `
            "Mesen"

        $contextStreams = if ($routine.Pc -eq "F1B4") {
            @(
                [pscustomobject] @{
                    ContextY = "00"; AllowedFrameOffsets = @(1)
                },
                [pscustomobject] @{
                    ContextY = "06"
                    AllowedFrameOffsets = @(0, 1, 2, 5, 6, 18, 69)
                }
            )
        } elseif ($routine.Pc -eq "DD09") {
            @(
                [pscustomobject] @{
                    ContextY = "00"; AllowedFrameOffsets = @(0, 1, 4)
                },
                [pscustomobject] @{
                    ContextY = "06"
                    AllowedFrameOffsets = @(0, 1, 2, 3, 4, 5, 8, 10)
                }
            )
        } else {
            @(
                [pscustomobject] @{
                    ContextY = $null
                    AllowedFrameOffsets = @(0, 1, 2, 5, 6, 18, 69)
                }
            )
        }

        foreach ($context in $contextStreams) {
            $beetleEntries =
                Get-RoutineRows $beetle $routine.Bank $routine.Pc
            $mesenEntries =
                Get-RoutineRows $mesen $routine.Bank $routine.Pc
            $beetleRoutineCalls =
                Get-RoutineRows $beetleCalls $routine.Bank $routine.Pc
            $mesenRoutineCalls =
                Get-RoutineRows $mesenCalls $routine.Bank $routine.Pc
            $streamName = $name
            if ($null -ne $context.ContextY) {
                $beetleEntries = @(
                    $beetleEntries |
                        Where-Object { $_.y -eq $context.ContextY }
                )
                $mesenEntries = @(
                    $mesenEntries |
                        Where-Object { $_.y -eq $context.ContextY }
                )
                $beetleRoutineCalls = @(
                    $beetleRoutineCalls |
                        Where-Object { $_.input_y -eq $context.ContextY }
                )
                $mesenRoutineCalls = @(
                    $mesenRoutineCalls |
                        Where-Object { $_.input_y -eq $context.ContextY }
                )
                $streamName =
                    "$name caller-context Y=$($context.ContextY)"
            }

            Assert-AlignedRows `
                $beetleEntries `
                @(
                    $mesenEntries |
                        Select-Object -First $beetleEntries.Count
                ) `
                0 `
                1 `
                $streamName `
                -AllowedFrameOffsets $context.AllowedFrameOffsets

            if ($routine.Pc -eq "F241" -or $routine.Pc -eq "D1A2") {
                Compare-PhysicsLeafCalls `
                    $beetleRoutineCalls `
                    @(
                        $mesenRoutineCalls |
                            Select-Object -First $beetleRoutineCalls.Count
                    ) `
                    $beetleWrites `
                    $mesenWrites `
                    $streamName
            } elseif (
                $beetleRoutineCalls.Count -ne $beetleEntries.Count -or
                $mesenRoutineCalls.Count -ne $mesenEntries.Count) {
                throw "$streamName has an incomplete bounded call."
            } else {
                $mesenStructuralCalls = @(
                    $mesenRoutineCalls |
                        Select-Object -First $beetleRoutineCalls.Count
                )
                for ($index = 0;
                     $index -lt $beetleRoutineCalls.Count;
                     $index++) {
                    foreach ($field in @(
                        "caller_pc",
                        "return_pc",
                        "return_physical_bank",
                        "return_instruction_pc"
                    )) {
                        if ($beetleRoutineCalls[$index].$field -ne
                            $mesenStructuralCalls[$index].$field) {
                            throw (
                                "$streamName structural $field changed at " +
                                "call $($index + 1)."
                            )
                        }
                    }
                }
            }
        }
    }

    if ($script:physicsExactLeafCalls -ne 549 -or
        $script:physicsStateDivergentLeafCalls -ne 259) {
        throw (
            "Physics leaf classification changed: exact=" +
            "$script:physicsExactLeafCalls, pre-call-state-divergent=" +
            "$script:physicsStateDivergentLeafCalls."
        )
    }

    $callerSummary = @(
        $mesen |
            Group-Object physical_bank, logical_pc, caller_bank, caller_pc |
            Sort-Object Name |
            ForEach-Object {
                $parts = $_.Name.Split(",").Trim()
                [pscustomobject] @{
                    Routine = "$($parts[0]):$($parts[1])"
                    Caller = "$($parts[2]):$($parts[3])"
                    Calls = $_.Count
                }
            }
    )

    Write-Output "Physics routine comparison complete."
    Write-Output (
        "Caller-context input streams align; the two cores can place the " +
        "Y=06 slot-0 motion calls in different output frames."
    )
    Write-Output (
        "F241/D1A2 exact leaf scopes: $script:physicsExactLeafCalls; " +
        "pre-call-state-divergent leaf scopes: " +
        "$script:physicsStateDivergentLeafCalls."
    )
    Write-Output (
        "F1B4 and DD09 wrapper scopes have matching counts, callers, " +
        "returns, and caller-context input streams; their output/write " +
        "scopes require savestate-normalized pre-call replay because slot-0 " +
        "update scheduling differs after scripted flipper inputs."
    )
    $callerSummary | Format-Table -AutoSize
    return
}

Assert-Count $beetle "00" "E195" 1799 "Beetle"
Assert-Count $mesen "00" "E195" 1799 "Mesen"
Assert-Count $beetle "01" "C3CB" 860 "Beetle"
Assert-Count $mesen "01" "C3CB" 861 "Mesen"
Assert-Count $beetle "01" "C49E" 860 "Beetle"
Assert-Count $mesen "01" "C49E" 861 "Mesen"
Assert-Count $beetle "03" "6966" 19648 "Beetle"
Assert-Count $mesen "03" "6966" 19674 "Mesen"

$beetleController = Get-RoutineRows $beetle "00" "E195"
$mesenController = Get-RoutineRows $mesen "00" "E195"
for ($index = 0; $index -lt $beetleController.Count; $index++) {
    if ($beetleController[$index].frame -ne $mesenController[$index].frame) {
        throw "00:E195 frame sequence changed at call $($index + 1)."
    }
}

$beetleC3cb = Get-RoutineRows $beetle "01" "C3CB"
$mesenC3cb = Get-RoutineRows $mesen "01" "C3CB"
Assert-AlignedRows `
    $beetleC3cb `
    @($mesenC3cb | Select-Object -First $beetleC3cb.Count) `
    0 `
    1 `
    "01:C3CB"

$beetleC49e = Get-RoutineRows $beetle "01" "C49E"
$mesenC49e = Get-RoutineRows $mesen "01" "C49E"
Assert-AlignedRows `
    $beetleC49e `
    @($mesenC49e | Select-Object -First $beetleC49e.Count) `
    0 `
    1 `
    "01:C49E"

$beetle6966 = Get-RoutineRows $beetle "03" "6966"
$mesen6966 = Get-RoutineRows $mesen "03" "6966"
for ($index = 0; $index -lt 4; $index++) {
    $beetleRow = $beetle6966[$index]
    $mesenRow = $mesen6966[$index]
    if ([int] $beetleRow.frame -ne 873 -or [int] $mesenRow.frame -ne 873) {
        throw "03:6966 bootstrap calls moved from frame 873."
    }
    foreach ($field in @("a", "x", "y", "sp", "mpr0", "mpr1", "mpr2",
            "mpr3", "mpr4", "mpr5", "mpr6", "mpr7")) {
        if ($beetleRow.$field -ne $mesenRow.$field) {
            throw "03:6966 bootstrap $field changed."
        }
    }
    if (((Convert-HexByte $beetleRow.status) -band 0xEF) -ne
        ((Convert-HexByte $mesenRow.status) -band 0xEF)) {
        throw "03:6966 bootstrap status differs beyond the T flag."
    }
}
Assert-AlignedRows `
    @($beetle6966 | Select-Object -Skip 4) `
    @(
        $mesen6966 |
            Select-Object -Skip 4 -First ($beetle6966.Count - 4)
    ) `
    0 `
    1 `
    "03:6966"

$beetleC3cbCalls = Get-RoutineRows $beetleCalls "01" "C3CB"
$mesenC3cbCalls = Get-RoutineRows $mesenCalls "01" "C3CB"
Assert-AlignedCalls `
    $beetleC3cbCalls `
    @($mesenC3cbCalls | Select-Object -First $beetleC3cbCalls.Count) `
    $beetleWrites `
    $mesenWrites `
    1 `
    "01:C3CB"

$beetleC49eCalls = Get-RoutineRows $beetleCalls "01" "C49E"
$mesenC49eCalls = Get-RoutineRows $mesenCalls "01" "C49E"
Assert-AlignedCalls `
    $beetleC49eCalls `
    @($mesenC49eCalls | Select-Object -First $beetleC49eCalls.Count) `
    $beetleWrites `
    $mesenWrites `
    1 `
    "01:C49E"

$beetle6966Calls = Get-RoutineRows $beetleCalls "03" "6966"
$mesen6966Calls = Get-RoutineRows $mesenCalls "03" "6966"
Assert-AlignedCalls `
    @($beetle6966Calls | Select-Object -First 4) `
    @($mesen6966Calls | Select-Object -First 4) `
    $beetleWrites `
    $mesenWrites `
    0 `
    "03:6966 bootstrap"
Assert-AlignedCalls `
    @($beetle6966Calls | Select-Object -Skip 4) `
    @(
        $mesen6966Calls |
            Select-Object -Skip 4 -First ($beetle6966Calls.Count - 4)
    ) `
    $beetleWrites `
    $mesenWrites `
    1 `
    "03:6966"

$callerSummary = @(
    $mesen |
        Group-Object physical_bank, logical_pc, caller_bank, caller_pc |
        Sort-Object Name |
        ForEach-Object {
            $parts = $_.Name.Split(",").Trim()
            [pscustomobject] @{
                Routine = "$($parts[0]):$($parts[1])"
                Caller = "$($parts[2]):$($parts[3])"
                Calls = $_.Count
            }
        }
)

Write-Output "Gameplay routine comparison complete."
Write-Output (
    "The apparent deltas are exactly one frame of phase: " +
    "Beetle frame n matches Mesen frame n-1 after table activation."
)
Write-Output (
    "Aligned A/X/Y/SP and stable MPR values match; status differences are " +
    "limited to HuC6280 T (0x10)."
)
Write-Output (
    "Aligned return paths and outputs match. Exact bounded calls: " +
    "$script:exactBoundedCalls; interrupt-relocated scopes: " +
    "$script:interruptRelocatedCalls."
)
Write-Output (
    "Cycle budgets and ordered work-RAM writes match exactly whenever the " +
    "two cores place their periodic interrupt outside the same call scope."
)
Write-Output (
    "Interrupt-internal writes with core-specific callback-cycle attribution: " +
    "$script:interruptInternalWrites."
)
$callerSummary | Format-Table -AutoSize
