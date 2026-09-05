# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
    Ranks native-port candidates from a whole-ROM instruction cycle profile.

.DESCRIPTION
    Prefer the registry's `.interpreter.csv` companion when ranking remaining
    porting work. It records actual interpreted intervals; omit entry and seam
    filters to retain expensive guarded, core and host boundaries. Whole-ROM
    profiles also contain accepted native transaction costs and serve a
    different measurement. The two scopes cannot be merged.

    `--instruction-profile` records every executed instruction address and the
    master cycles attributed to it, with no keyed observer set. This script
    turns that flat profile into candidates: it groups contiguous executed
    addresses into approximate routines, ranks them by master cycles, and marks
    the ones the admitted registry already enters.

    Unlike Analyze-NativeFallbackHotspots.ps1, which ranks within a bounded set
    of addresses someone already chose to observe, this ranks the whole game.
    Use it to pick the neighbourhood; use the fallback hotspot tool to profile
    a chosen family in detail.

.PARAMETER ExcludeAddress
    Addresses to drop before ranking, as BANK:PC. Use it to remove code that is
    not a porting target so the ranking reflects addressable work -- most
    importantly the frame-sync spin loop, which is the single largest consumer
    of master cycles in the game and is the machine waiting, not working.

.PARAMETER UnregisteredOnly
    Drop admitted registry entry points so the result is the instruction work
    still interpreted by the deployed registry.

.PARAMETER ExcludeFrameSyncWait
    Drop 00:E321-E32B, whose cycles represent waiting for the raster rather
    than computation available to port.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]] $ProfilePath,

    [string] $RegistryPath,

    # Registry entries do not cover internal core or host boundaries. This
    # manifest classifies those addresses before a profile is used to choose
    # another native port.
    [string] $SeamManifestPath,

    [int] $Top = 25,

    # Two executed addresses closer than this are treated as one routine.
    [int] $MaximumGap = 4,

    [string[]] $ExcludeAddress = @(),

    # Show only instructions that are not admitted registry entry points.
    # This is the deployed-residue view used after the registry is mature.
    [switch] $UnregisteredOnly,

    # Select only unregistered addresses that have not already been classified
    # as a core, host, or guarded-native boundary.
    [switch] $CandidateOnly,

    # The game spends most of its time polling the frame boundary at
    # 00:E321-E32B. Those cycles are real machine time but not porting work.
    [switch] $ExcludeFrameSyncWait
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $RegistryPath) {
    $repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
    $RegistryPath = Join-Path $repositoryRoot "src/app/native_registry_table.inc"
}
if (-not $SeamManifestPath) {
    $SeamManifestPath = Join-Path $PSScriptRoot "native_execution_seams.csv"
}

foreach ($path in $ProfilePath) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Instruction profile not found: $path"
    }
}

$excluded = [System.Collections.Generic.HashSet[string]]::new()
foreach ($address in $ExcludeAddress) {
    $excluded.Add($address.ToUpperInvariant()) | Out-Null
}

$registry = [System.Collections.Generic.HashSet[string]]::new()
if (Test-Path -LiteralPath $RegistryPath) {
    $pattern = 'DC_ADMITTED_ROUTINE\(0x(?<bank>[0-9A-Fa-f]{2})U, 0x(?<pc>[0-9A-Fa-f]{4})U'
    foreach ($match in [regex]::Matches(
            (Get-Content -LiteralPath $RegistryPath -Raw), $pattern)) {
        $registry.Add(
            ("{0}:{1}" -f
                $match.Groups["bank"].Value.ToUpperInvariant(),
                $match.Groups["pc"].Value.ToUpperInvariant())) | Out-Null
    }
}

$seams = [System.Collections.Generic.List[object]]::new()
if (Test-Path -LiteralPath $SeamManifestPath -PathType Leaf) {
    $requiredSeamColumns = @("Bank", "Start", "End", "Kind", "Detail")
    foreach ($row in (Import-Csv -LiteralPath $SeamManifestPath)) {
        foreach ($column in $requiredSeamColumns) {
            if (-not ($row.PSObject.Properties.Name -contains $column)) {
                throw "Execution-seam manifest lacks '$column': $SeamManifestPath"
            }
        }
        if ($row.Kind -notin @("core", "host", "guarded")) {
            throw "Execution seam has unsupported kind '$($row.Kind)': $SeamManifestPath"
        }
        try {
            $start = [Convert]::ToInt32($row.Start, 16)
            $end = [Convert]::ToInt32($row.End, 16)
            if ($start -gt $end) {
                throw "start is greater than end"
            }
            $seams.Add([pscustomobject] @{
                Bank = $row.Bank.ToUpperInvariant()
                Start = $start
                End = $end
                Kind = $row.Kind
                Detail = $row.Detail
            }) | Out-Null
        } catch {
            throw "Invalid execution seam '$($row.Bank):$($row.Start)-$($row.End)': $($_.Exception.Message)"
        }
    }
}

function Find-ExecutionSeam {
    param([string] $Bank, [int] $Pc)

    foreach ($seam in $seams) {
        if ($seam.Bank -eq $Bank -and $Pc -ge $seam.Start -and $Pc -le $seam.End) {
            return $seam
        }
    }
    return $null
}

$samplesByAddress = @{}
$profileScopes = [System.Collections.Generic.HashSet[string]]::new()
foreach ($path in $ProfilePath) {
    $profileLines = @(Get-Content -LiteralPath $path)
    $scope = if ($profileLines -contains "# scope=interpreter-residue") {
        "interpreter-residue"
    } else {
        "whole-ROM"
    }
    [void] $profileScopes.Add($scope)
    if ($profileScopes.Count -gt 1) {
        throw "Do not merge whole-ROM profiles with interpreter-residue profiles. They measure different work."
    }
    $rows = @(
        $profileLines |
            Where-Object { -not $_.StartsWith("#") } |
            ConvertFrom-Csv)
    if ($rows.Count -eq 0) {
        throw "Instruction profile is empty: $path"
    }

    foreach ($row in $rows) {
        $address = "{0}:{1}" -f
            $row.physical_bank.ToUpperInvariant(),
            $row.logical_pc.ToUpperInvariant()
        $isFrameSyncWait =
            $row.physical_bank.ToUpperInvariant() -eq "00" -and
            [Convert]::ToInt32($row.logical_pc, 16) -ge 0xE321 -and
            [Convert]::ToInt32($row.logical_pc, 16) -le 0xE32B
        $profileRegistry =
            $row.PSObject.Properties.Name -contains "registry_entry" -and
            $row.registry_entry -eq "1"
        $pc = [Convert]::ToInt32($row.logical_pc, 16)
        $seam = Find-ExecutionSeam $row.physical_bank.ToUpperInvariant() $pc
        if (-not $samplesByAddress.ContainsKey($address)) {
            $samplesByAddress[$address] = [pscustomobject] @{
                Address = $address
                Bank = $row.physical_bank.ToUpperInvariant()
                Pc = $pc
                Cycles = [uint64] 0
                Executions = [uint64] 0
                Registry = $false
                SeamKind = if ($null -eq $seam) { "candidate" } else { $seam.Kind }
                SeamDetail = if ($null -eq $seam) { "" } else { $seam.Detail }
                Excluded = $excluded.Contains($address) -or
                    ($ExcludeFrameSyncWait -and $isFrameSyncWait)
            }
        }
        $sample = $samplesByAddress[$address]
        $sample.Cycles += [uint64] $row.master_cycles
        $sample.Executions += [uint64] $row.executions
        $sample.Registry = $sample.Registry -or
            $profileRegistry -or $registry.Contains($address)
    }
}

$samples = @($samplesByAddress.Values)
$isInterpreterResidue = $profileScopes.Contains("interpreter-residue")

$total = ($samples | Measure-Object -Property Cycles -Sum).Sum
$ranked = @(
    $samples |
        Where-Object {
            -not $_.Excluded -and
            ((-not $UnregisteredOnly -and -not $CandidateOnly) -or -not $_.Registry) -and
            (-not $CandidateOnly -or $_.SeamKind -eq "candidate")
        }
)
$addressable = ($ranked | Measure-Object -Property Cycles -Sum).Sum

Write-Output ("Profiles merged    : {0:N0}" -f $ProfilePath.Count)
Write-Output ("Addresses executed : {0:N0}" -f $samples.Count)
Write-Output ("Total cycles       : {0:N0}" -f $total)
if ($excluded.Count -ne 0 -or $ExcludeFrameSyncWait -or $UnregisteredOnly -or $CandidateOnly) {
    Write-Output ("Filtered cycles    : {0:N0} ({1:N1}%)" -f
        ($total - $addressable), (100.0 * ($total - $addressable) / $total))
}
Write-Output ("Addressable cycles : {0:N0}" -f $addressable)
if ($isInterpreterResidue) {
    Write-Output "Measurement       : actual interpreter intervals (native transcript bodies excluded)"
}
if ($CandidateOnly) {
    Write-Output "View              : unclassified deployed candidates"
} elseif ($UnregisteredOnly) {
    Write-Output "View              : unregistered deployed residue (entry-only)"
} elseif ($isInterpreterResidue) {
    Write-Output "View              : all remaining interpreter seams, including guarded/core/host"
}

$seamAccounting = @(
    $samples |
        Where-Object { -not $_.Excluded } |
        Group-Object SeamKind |
        ForEach-Object {
            [pscustomobject] @{
                Kind = $_.Name
                Cycles = ($_.Group | Measure-Object -Property Cycles -Sum).Sum
            }
        } |
        Sort-Object Cycles -Descending
)
Write-Output "Execution-boundary accounting"
foreach ($classification in $seamAccounting) {
    Write-Output ("  {0,-9} {1,15:N0}" -f $classification.Kind, $classification.Cycles)
}

# Group contiguous executed addresses within a bank into one candidate.
$clusters = [System.Collections.Generic.List[object]]::new()
foreach ($bank in @($ranked | Group-Object Bank)) {
    $current = $null
    foreach ($sample in @($bank.Group | Sort-Object Pc)) {
        if ($null -ne $current -and
            ($sample.Pc - $current.End) -le $MaximumGap) {
            $current.End = $sample.Pc
            $current.Cycles += $sample.Cycles
            $current.Addresses += 1
            if ($sample.Registry) {
                $current.Registry = $true
            }
            [void] $current.SeamKinds.Add($sample.SeamKind)
            continue
        }
        if ($null -ne $current) {
            $clusters.Add($current) | Out-Null
        }
        $current = [pscustomobject] @{
            Bank = $sample.Bank
            Start = $sample.Pc
            End = $sample.Pc
            Cycles = $sample.Cycles
            Addresses = 1
            Registry = $sample.Registry
            SeamKinds = [System.Collections.Generic.HashSet[string]]::new()
        }
        [void] $current.SeamKinds.Add($sample.SeamKind)
    }
    if ($null -ne $current) {
        $clusters.Add($current) | Out-Null
    }
}

Write-Output ""
Write-Output ("Top {0} candidate routines (contiguous executed ranges)" -f $Top)
Write-Output "  range                  cycles     share  addrs  registry  classification"
foreach ($cluster in @($clusters | Sort-Object Cycles -Descending |
        Select-Object -First $Top)) {
    $classification = @($cluster.SeamKinds | Sort-Object) -join "+"
    Write-Output ("  {0}:{1:X4}-{2:X4} {3,15:N0} {4,7:N2}% {5,6}  {6}" -f
        $cluster.Bank,
        $cluster.Start,
        $cluster.End,
        $cluster.Cycles,
        (100.0 * $cluster.Cycles / $addressable),
        $cluster.Addresses,
        $(if ($cluster.Registry) { "entered" } else { "--" }) +
        ("  {0}" -f $classification))
}

Write-Output ""
if ($CandidateOnly) {
    Write-Output ("Unclassified deployed candidate clusters: {0}" -f
        $clusters.Count)
} elseif ($UnregisteredOnly) {
    Write-Output ("Unregistered deployed-residue clusters: {0}" -f
        $clusters.Count)
} elseif ($isInterpreterResidue) {
    Write-Output ("Remaining interpreter clusters: {0}; registry marks entry ownership, not native coverage." -f $clusters.Count)
} else {
    $coveredClusters = @($clusters | Where-Object Registry)
    $coveredMeasure = $coveredClusters |
        Measure-Object -Property Cycles -Sum
    $covered = if ($null -eq $coveredMeasure.Sum) {
        [uint64] 0
    } else {
        [uint64] $coveredMeasure.Sum
    }
    Write-Output ("Clusters entered by the registry: {0} of {1}, {2:N1}% of addressable cycles" -f
        $coveredClusters.Count,
        $clusters.Count,
        (100.0 * $covered / $addressable))
}
