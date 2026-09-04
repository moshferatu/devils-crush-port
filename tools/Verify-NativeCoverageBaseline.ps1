# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ManifestPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Native coverage baseline does not exist: $ManifestPath"
}

$expectedRoutes = @(
    "gameplay", "alternate", "sweep", "crossfire", "orbit", "lanes",
    "stagger", "pulse", "rally", "slalom", "pendulum", "nudge",
    "switchback", "spiral", "ricochet", "volley", "whirlpool", "cascade",
    "higharc", "twist", "sidestep", "weave", "bankshot", "playfield",
    "corner", "alley", "rebound", "skew", "helix", "vector", "radial"
)
$rows = @(Import-Csv -LiteralPath $ManifestPath)
if ($rows.Count -ne $expectedRoutes.Count) {
    throw "Expected $($expectedRoutes.Count) coverage rows, found $($rows.Count)."
}

$actualRoutes = @($rows | ForEach-Object Route)
if (Compare-Object $expectedRoutes $actualRoutes) {
    throw "Coverage baseline routes differ from the deterministic corpus."
}

foreach ($row in $rows) {
    foreach ($property in @("Route", "AssemblyStateSha256", "RegistryStateSha256")) {
        if (-not ($row.PSObject.Properties.Name -contains $property)) {
            throw "Coverage baseline lacks '$property'."
        }
    }
    foreach ($hash in @($row.AssemblyStateSha256, $row.RegistryStateSha256)) {
        if ($hash -notmatch '^[0-9a-f]{64}$') {
            throw "Coverage baseline has an invalid SHA-256 for '$($row.Route)'."
        }
    }
}

Write-Output "Native coverage baseline verified: $($rows.Count) deterministic routes."
