# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
    Ranks native-port candidates from a whole-ROM instruction cycle profile.

.DESCRIPTION
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
    [string] $ProfilePath,

    [string] $RegistryPath,

    [int] $Top = 25,

    # Two executed addresses closer than this are treated as one routine.
    [int] $MaximumGap = 4,

    [string[]] $ExcludeAddress = @(),

    # Show only instructions that are not admitted registry entry points.
    # This is the deployed-residue view used after the registry is mature.
    [switch] $UnregisteredOnly,

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

if (-not (Test-Path -LiteralPath $ProfilePath)) {
    throw "Instruction profile not found: $ProfilePath"
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

$rows = @(
    Get-Content -LiteralPath $ProfilePath |
        Where-Object { -not $_.StartsWith("#") } |
        ConvertFrom-Csv)
if ($rows.Count -eq 0) {
    throw "Instruction profile is empty: $ProfilePath"
}

$samples = [System.Collections.Generic.List[object]]::new()
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
    $samples.Add(
        [pscustomobject] @{
            Address = $address
            Bank = $row.physical_bank.ToUpperInvariant()
            Pc = [Convert]::ToInt32($row.logical_pc, 16)
            Cycles = [uint64] $row.master_cycles
            Executions = [uint64] $row.executions
            Registry = $profileRegistry -or $registry.Contains($address)
            Excluded = $excluded.Contains($address) -or
                ($ExcludeFrameSyncWait -and $isFrameSyncWait)
        }) | Out-Null
}

$total = ($samples | Measure-Object -Property Cycles -Sum).Sum
$ranked = @(
    $samples |
        Where-Object {
            -not $_.Excluded -and
            (-not $UnregisteredOnly -or -not $_.Registry)
        }
)
$addressable = ($ranked | Measure-Object -Property Cycles -Sum).Sum

Write-Host ("Addresses executed : {0:N0}" -f $samples.Count)
Write-Host ("Total cycles       : {0:N0}" -f $total)
if ($excluded.Count -ne 0 -or $ExcludeFrameSyncWait -or $UnregisteredOnly) {
    Write-Host ("Filtered cycles    : {0:N0} ({1:N1}%)" -f
        ($total - $addressable), (100.0 * ($total - $addressable) / $total))
}
Write-Host ("Addressable cycles : {0:N0}" -f $addressable)
if ($UnregisteredOnly) {
    Write-Host "View              : unregistered deployed residue"
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
        }
    }
    if ($null -ne $current) {
        $clusters.Add($current) | Out-Null
    }
}

Write-Host ""
Write-Host ("Top {0} candidate routines (contiguous executed ranges)" -f $Top)
Write-Host "  range                  cycles     share  addrs  registry"
foreach ($cluster in @($clusters | Sort-Object Cycles -Descending |
        Select-Object -First $Top)) {
    Write-Host ("  {0}:{1:X4}-{2:X4} {3,15:N0} {4,7:N2}% {5,6}  {6}" -f
        $cluster.Bank,
        $cluster.Start,
        $cluster.End,
        $cluster.Cycles,
        (100.0 * $cluster.Cycles / $addressable),
        $cluster.Addresses,
        $(if ($cluster.Registry) { "entered" } else { "--" }))
}

Write-Host ""
if ($UnregisteredOnly) {
    Write-Host ("Unregistered deployed-residue clusters: {0}" -f
        $clusters.Count)
} else {
    $coveredClusters = @($clusters | Where-Object Registry)
    $coveredMeasure = $coveredClusters |
        Measure-Object -Property Cycles -Sum
    $covered = if ($null -eq $coveredMeasure.Sum) {
        [uint64] 0
    } else {
        [uint64] $coveredMeasure.Sum
    }
    Write-Host ("Clusters entered by the registry: {0} of {1}, {2:N1}% of addressable cycles" -f
        $coveredClusters.Count,
        $clusters.Count,
        (100.0 * $covered / $addressable))
}
