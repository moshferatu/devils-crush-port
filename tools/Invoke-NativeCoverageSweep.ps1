# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
Runs the deterministic table-route corpus as an assembly/native A/B sweep.

.DESCRIPTION
Each selected route is recorded twice: once with the original assembly path
and once with the full admitted registry. Both runs retain whole-ROM
instruction profiles, and the deployed profiles are merged into one
unregistered-residue ranking for the next native port candidate. A committed
baseline manifest makes either route's state hash a hard regression gate.

The script refuses a non-empty output directory. A coverage run is evidence,
not a cache: retaining both sides of each comparison makes a later changed
hash or candidate choice inspectable.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $App,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $Rom,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string] $OutputDirectory,

    [ValidateRange(1, 500)]
    [int] $Top = 25,

    # A prior manifest from an accepted run. Each assembly and registry state
    # hash must match its corresponding route before the sweep can pass.
    [string] $BaselineManifest = "",

    # Use only while deliberately generating a reviewed replacement baseline.
    [switch] $SkipBaselineCheck,

    # The direct assembly/native comparison is stricter than the current
    # registry-wide fidelity contract. Keep it available for a fully proven
    # configuration without preventing the suite from recording today's
    # independent A/B evidence.
    [switch] $RequireAssemblyRegistryMatch,

    # Omit to run the complete deterministic table-route corpus.
    [string[]] $Route = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$routeDefinitions = @(
    [pscustomobject] @{ Name = "gameplay"; Flag = "--record-gameplay" },
    [pscustomobject] @{ Name = "alternate"; Flag = "--record-gameplay-alternate" },
    [pscustomobject] @{ Name = "sweep"; Flag = "--record-gameplay-sweep" },
    [pscustomobject] @{ Name = "crossfire"; Flag = "--record-gameplay-crossfire" },
    [pscustomobject] @{ Name = "orbit"; Flag = "--record-gameplay-orbit" },
    [pscustomobject] @{ Name = "lanes"; Flag = "--record-gameplay-lanes" },
    [pscustomobject] @{ Name = "stagger"; Flag = "--record-gameplay-stagger" },
    [pscustomobject] @{ Name = "pulse"; Flag = "--record-gameplay-pulse" },
    [pscustomobject] @{ Name = "rally"; Flag = "--record-gameplay-rally" },
    [pscustomobject] @{ Name = "slalom"; Flag = "--record-gameplay-slalom" },
    [pscustomobject] @{ Name = "pendulum"; Flag = "--record-gameplay-pendulum" },
    [pscustomobject] @{ Name = "nudge"; Flag = "--record-gameplay-nudge" },
    [pscustomobject] @{ Name = "switchback"; Flag = "--record-gameplay-switchback" },
    [pscustomobject] @{ Name = "spiral"; Flag = "--record-gameplay-spiral" },
    [pscustomobject] @{ Name = "ricochet"; Flag = "--record-gameplay-ricochet" },
    [pscustomobject] @{ Name = "volley"; Flag = "--record-gameplay-volley" },
    [pscustomobject] @{ Name = "whirlpool"; Flag = "--record-gameplay-whirlpool" },
    [pscustomobject] @{ Name = "cascade"; Flag = "--record-gameplay-cascade" },
    [pscustomobject] @{ Name = "higharc"; Flag = "--record-gameplay-higharc" },
    [pscustomobject] @{ Name = "twist"; Flag = "--record-gameplay-twist" },
    [pscustomobject] @{ Name = "sidestep"; Flag = "--record-gameplay-sidestep" },
    [pscustomobject] @{ Name = "weave"; Flag = "--record-gameplay-weave" },
    [pscustomobject] @{ Name = "bankshot"; Flag = "--record-gameplay-bankshot" },
    [pscustomobject] @{ Name = "playfield"; Flag = "--record-gameplay-playfield" },
    [pscustomobject] @{ Name = "corner"; Flag = "--record-gameplay-corner" },
    [pscustomobject] @{ Name = "alley"; Flag = "--record-gameplay-alley" },
    [pscustomobject] @{ Name = "rebound"; Flag = "--record-gameplay-rebound" },
    [pscustomobject] @{ Name = "skew"; Flag = "--record-gameplay-skew" },
    [pscustomobject] @{ Name = "helix"; Flag = "--record-gameplay-helix" },
    [pscustomobject] @{ Name = "vector"; Flag = "--record-gameplay-vector" },
    [pscustomobject] @{ Name = "radial"; Flag = "--record-gameplay-radial" }
)

function Resolve-ExistingFile {
    param([string] $Path, [string] $Label)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label does not exist: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Invoke-RouteRecording {
    param(
        [pscustomobject] $Definition,
        [string] $Mode,
        [switch] $ReplaceRegistry
    )

    $modeDirectory = Join-Path $root $Mode
    $statePath = Join-Path $modeDirectory ("{0}.state.csv" -f $Definition.Name)
    $profilePath = Join-Path $modeDirectory ("{0}.instruction-profile.csv" -f $Definition.Name)
    $logPath = Join-Path $modeDirectory ("{0}.log.txt" -f $Definition.Name)
    $arguments = @(
        $Definition.Flag,
        $statePath,
        "--instruction-profile",
        $profilePath,
        "--rom",
        $resolvedRom
    )
    if ($ReplaceRegistry) {
        $arguments += "--replace-registry"
    }

    $output = & $resolvedApp @arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Set-Content -LiteralPath $logPath
    if ($exitCode -ne 0) {
        throw (
            "${Mode} recording for '$($Definition.Name)' failed " +
            "with exit code $exitCode. See $logPath"
        )
    }
    foreach ($artifact in @($statePath, $profilePath)) {
        if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
            throw "${Mode} recording for '$($Definition.Name)' omitted $artifact."
        }
    }
    return [pscustomobject] @{
        StatePath = $statePath
        ProfilePath = $profilePath
        StateHash = (Get-FileHash -LiteralPath $statePath -Algorithm SHA256).Hash.ToLowerInvariant()
        ProfileHash = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$resolvedApp = Resolve-ExistingFile $App "Application"
$resolvedRom = Resolve-ExistingFile $Rom "ROM"
$baseline = @{}
if ([string]::IsNullOrWhiteSpace($BaselineManifest) -and -not $SkipBaselineCheck) {
    $BaselineManifest = Join-Path $PSScriptRoot "..\tests\native_coverage_table_routes.csv"
}
if (-not [string]::IsNullOrWhiteSpace($BaselineManifest)) {
    $resolvedBaseline = Resolve-ExistingFile $BaselineManifest "Baseline manifest"
    foreach ($row in (Import-Csv -LiteralPath $resolvedBaseline)) {
        foreach ($property in @(
                "Route", "AssemblyStateSha256", "RegistryStateSha256")) {
            if (-not ($row.PSObject.Properties.Name -contains $property)) {
                throw "Baseline manifest lacks '$property': $resolvedBaseline"
            }
        }
        if ($baseline.ContainsKey($row.Route)) {
            throw "Baseline manifest repeats route '$($row.Route)': $resolvedBaseline"
        }
        $baseline[$row.Route] = $row
    }
}
$root = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $root) {
    $existing = @(Get-ChildItem -LiteralPath $root -Force)
    if ($existing.Count -ne 0) {
        throw "Coverage output directory must be empty: $root"
    }
} else {
    [void] (New-Item -ItemType Directory -Path $root)
}

$selectedRoutes = @(
    if ($Route.Count -eq 0) {
        $routeDefinitions
    } else {
        $requested = [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::OrdinalIgnoreCase)
        foreach ($name in $Route) {
            [void] $requested.Add($name)
        }
        $knownNames = @($routeDefinitions | ForEach-Object Name)
        $unknown = @($requested | Where-Object { $_ -notin $knownNames })
        if ($unknown.Count -ne 0) {
            throw "Unknown coverage route(s): $($unknown -join ', ')"
        }
        $routeDefinitions | Where-Object { $requested.Contains($_.Name) }
    }
)

foreach ($mode in @("assembly", "registry")) {
    [void] (New-Item -ItemType Directory -Path (Join-Path $root $mode))
}

$manifest = [System.Collections.Generic.List[object]]::new()
$registryProfiles = [System.Collections.Generic.List[string]]::new()
foreach ($definition in $selectedRoutes) {
    Write-Host ("[{0}/{1}] {2}" -f
        ($manifest.Count + 1), $selectedRoutes.Count, $definition.Name)
    $assembly = Invoke-RouteRecording $definition "assembly"
    $registry = Invoke-RouteRecording $definition "registry" -ReplaceRegistry
    $match = $assembly.StateHash -eq $registry.StateHash
    $manifest.Add([pscustomobject] @{
        Route = $definition.Name
        StateMatch = $match
        AssemblyStateSha256 = $assembly.StateHash
        RegistryStateSha256 = $registry.StateHash
        AssemblyProfileSha256 = $assembly.ProfileHash
        RegistryProfileSha256 = $registry.ProfileHash
    }) | Out-Null
    $registryProfiles.Add($registry.ProfilePath) | Out-Null
    if ($baseline.ContainsKey($definition.Name)) {
        $expected = $baseline[$definition.Name]
        $assemblyMatchesBaseline =
            $assembly.StateHash -eq $expected.AssemblyStateSha256.ToLowerInvariant()
        $registryMatchesBaseline =
            $registry.StateHash -eq $expected.RegistryStateSha256.ToLowerInvariant()
        if (-not $assemblyMatchesBaseline -or -not $registryMatchesBaseline) {
            throw (
                "State recording changed for '$($definition.Name)' relative to " +
                "the accepted baseline. See $root."
            )
        }
    } elseif ($baseline.Count -ne 0) {
        throw "Baseline manifest does not cover '$($definition.Name)'."
    }
    if ($RequireAssemblyRegistryMatch -and -not $match) {
        throw (
            "Registry changed the state recording for '$($definition.Name)'. " +
            "Assembly=$($assembly.StateHash); registry=$($registry.StateHash)."
        )
    }
}

$manifestPath = Join-Path $root "manifest.csv"
$manifest | Export-Csv -LiteralPath $manifestPath -NoTypeInformation

$residuePath = Join-Path $root "registry-unregistered-residue.md"
$candidateResiduePath = Join-Path $root "registry-candidate-residue.md"
$analyzer = Join-Path $PSScriptRoot "Analyze-InstructionProfile.ps1"
$analysis = & $analyzer `
    -ProfilePath $registryProfiles.ToArray() `
    -Top $Top `
    -UnregisteredOnly `
    -ExcludeFrameSyncWait 2>&1
$analysisExitCode = $LASTEXITCODE
$analysis | Set-Content -LiteralPath $residuePath
if ($analysisExitCode -ne 0) {
    throw "Residue analysis failed. See $residuePath"
}
$candidateAnalysis = & $analyzer `
    -ProfilePath $registryProfiles.ToArray() `
    -Top $Top `
    -CandidateOnly `
    -ExcludeFrameSyncWait 2>&1
$candidateAnalysisExitCode = $LASTEXITCODE
$candidateAnalysis | Set-Content -LiteralPath $candidateResiduePath
if ($candidateAnalysisExitCode -ne 0) {
    throw "Candidate residue analysis failed. See $candidateResiduePath"
}

Write-Host ""
if ($RequireAssemblyRegistryMatch) {
    Write-Host ("Coverage sweep passed: {0} routes, all assembly/registry state hashes match." -f $manifest.Count)
} else {
    $abDivergences = @($manifest | Where-Object { -not $_.StateMatch }).Count
    Write-Host ("Coverage sweep passed: {0} routes, {1} assembly/registry differences recorded." -f $manifest.Count, $abDivergences)
}
Write-Host "Manifest: $manifestPath"
Write-Host "Merged entry-only residue ranking: $residuePath"
Write-Host "Merged candidate ranking: $candidateResiduePath"
