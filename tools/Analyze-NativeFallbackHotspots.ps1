# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string[]] $CallPath,

    [string] $NativeRegistryPath = "",

    [string[]] $NativeRoutine = @(),

    [string[]] $AssemblyRoutine = @(),

    [ValidateRange(1, 500)]
    [int] $Top = 25,

    [switch] $AssemblyOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($NativeRegistryPath)) {
    $NativeRegistryPath = Join-Path $PSScriptRoot "..\src\app\native_replacements.cpp"
}

function ConvertTo-RoutineKey {
    param(
        [Parameter(Mandatory = $true)]
        [string] $PhysicalBank,

        [Parameter(Mandatory = $true)]
        [string] $LogicalPc
    )

    try {
        $bank = [Convert]::ToUInt32($PhysicalBank, 16)
        $pc = [Convert]::ToUInt32($LogicalPc, 16)
    } catch {
        throw "Invalid routine key ${PhysicalBank}:${LogicalPc}."
    }
    if ($bank -gt 0xFF -or $pc -gt 0xFFFF) {
        throw "Routine key ${PhysicalBank}:${LogicalPc} is outside its width."
    }
    return ("{0:X2}:{1:X4}" -f $bank, $pc)
}

function ConvertTo-DeclaredRoutineKey {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Value
    )

    if ($Value -notmatch '^(?<bank>[0-9A-Fa-f]{2}):(?<pc>[0-9A-Fa-f]{4})$') {
        throw "Routine '$Value' must use BANK:PC, for example 03:6966."
    }
    return ConvertTo-RoutineKey $Matches.bank $Matches.pc
}

function Import-RoutineCalls {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Routine-call trace does not exist: $Path"
    }
    $rows = @(
        Get-Content -LiteralPath $Path |
            Where-Object { -not $_.StartsWith("#") } |
            ConvertFrom-Csv
    )
    foreach ($row in $rows) {
        foreach ($property in @(
            "physical_bank", "logical_pc", "body_master_cycles", "write_count"
        )) {
            if (-not ($row.PSObject.Properties.Name -contains $property)) {
                throw "Routine-call trace '$Path' lacks '$property'."
            }
        }
    }
    return $rows
}

$nativeKeys = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
if (-not [string]::IsNullOrWhiteSpace($NativeRegistryPath)) {
    if (-not (Test-Path -LiteralPath $NativeRegistryPath -PathType Leaf)) {
        throw "Native registry source does not exist: $NativeRegistryPath"
    }
    $registry = Get-Content -LiteralPath $NativeRegistryPath -Raw
    $pattern = [regex]::new(
        '\.physical_bank\s*=\s*0x(?<bank>[0-9A-Fa-f]{2})U,\s*' +
        '\.logical_pc\s*=\s*0x(?<pc>[0-9A-Fa-f]{4})U'
    )
    foreach ($match in $pattern.Matches($registry)) {
        [void] $nativeKeys.Add(
            (ConvertTo-RoutineKey $match.Groups["bank"].Value $match.Groups["pc"].Value)
        )
    }
}
foreach ($routine in $NativeRoutine) {
    [void] $nativeKeys.Add((ConvertTo-DeclaredRoutineKey $routine))
}

$assemblyKeys = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
foreach ($routine in $AssemblyRoutine) {
    [void] $assemblyKeys.Add((ConvertTo-DeclaredRoutineKey $routine))
}

$hotspots = @{}
$totalCalls = [uint64] 0
foreach ($path in $CallPath) {
    foreach ($row in (Import-RoutineCalls $path)) {
        $key = ConvertTo-RoutineKey $row.physical_bank $row.logical_pc
        try {
            $cycles = [uint64] $row.body_master_cycles
            $writes = [uint64] $row.write_count
        } catch {
            throw "Routine-call trace '$path' has non-numeric cost data for $key."
        }
        if (-not $hotspots.ContainsKey($key)) {
            $hotspots[$key] = [pscustomobject] @{
                Routine = $key
                Calls = [uint64] 0
                TotalMasterCycles = [uint64] 0
                MaxMasterCycles = [uint64] 0
                OrderedWrites = [uint64] 0
            }
        }
        $entry = $hotspots[$key]
        $entry.Calls += 1
        $entry.TotalMasterCycles += $cycles
        $entry.MaxMasterCycles = [Math]::Max($entry.MaxMasterCycles, $cycles)
        $entry.OrderedWrites += $writes
        $totalCalls += 1
    }
}
if ($hotspots.Count -eq 0) {
    throw "No routine calls were present in the supplied traces."
}

# The ranking primarily represents emulated time removed by a native port.
# The small ordered-write premium breaks ties toward routines that also carry
# a larger fidelity/testing burden; it is not a replacement for cycle cost.
$ranked = @(
    $hotspots.Values |
        ForEach-Object {
            $status = if ($assemblyKeys.Contains($_.Routine)) {
                "Assembly candidate"
            } elseif ($nativeKeys.Contains($_.Routine)) {
                "Native registered"
            } else {
                "Assembly candidate"
            }
            [pscustomobject] @{
                Routine = $_.Routine
                Status = $status
                Calls = $_.Calls
                TotalMasterCycles = $_.TotalMasterCycles
                AverageMasterCycles = [Math]::Round(
                    [double] $_.TotalMasterCycles / [double] $_.Calls,
                    1
                )
                MaxMasterCycles = $_.MaxMasterCycles
                OrderedWrites = $_.OrderedWrites
                WeightedCost = $_.TotalMasterCycles + (12 * $_.OrderedWrites)
            }
        } |
        Where-Object { -not $AssemblyOnly -or $_.Status -eq "Assembly candidate" } |
        Sort-Object -Property WeightedCost, TotalMasterCycles, Calls -Descending
)
if ($ranked.Count -eq 0) {
    throw "No assembly candidates remain after applying the supplied overrides."
}
$totalWeightedCost = [uint64](
    ($ranked | Measure-Object -Property WeightedCost -Sum).Sum
)

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Native fallback hotspot profile")
$lines.Add("")
$lines.Add(
    "Observed $totalCalls call(s) across $($CallPath.Count) trace file(s). " +
    "The weighted cost is total master cycles plus 12 per ordered write; " +
    "it ranks observed payoff and fidelity complexity, not whole-game coverage."
)
$lines.Add("")
$lines.Add("| Rank | Routine | Registry status | Calls | Total master cycles | Avg. cycles | Max. cycles | Writes | Weighted cost | Route share |")
$lines.Add("| ---: | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
$rank = 0
foreach ($entry in ($ranked | Select-Object -First $Top)) {
    ++$rank
    $share = if ($totalWeightedCost -eq 0) {
        0.0
    } else {
        100.0 * [double] $entry.WeightedCost / [double] $totalWeightedCost
    }
    $lines.Add((
        "| $rank | ``$($entry.Routine)`` | $($entry.Status) | $($entry.Calls) | " +
        "$($entry.TotalMasterCycles) | $($entry.AverageMasterCycles) | " +
        "$($entry.MaxMasterCycles) | $($entry.OrderedWrites) | " +
        "$($entry.WeightedCost) | $([Math]::Round($share, 1))% |"
    ))
}
$lines.Add("")
$lines.Add(
    "`Native registered` is derived from ``$NativeRegistryPath`` and means " +
    "the dispatch registry admits that address. Individual state branches may " +
    "still conservatively fall back to assembly. Supply ``-AssemblyRoutine`` " +
    "to force a known fallback classification."
)
$lines.Add("")
$lines.Add(
    "The runtime records bounded calls only for requested ``--observe-hook`` " +
    "addresses. Build the observer set from one feature family or from a " +
    "candidate batch; this tool does not claim an unobserved ROM-wide profile."
)

$lines -join [Environment]::NewLine
