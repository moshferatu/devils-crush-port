# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ManifestPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Execution-seam manifest not found: $ManifestPath"
}

$rows = @(Import-Csv -LiteralPath $ManifestPath)
if ($rows.Count -eq 0) {
    throw "Execution-seam manifest is empty: $ManifestPath"
}
$requiredColumns = @("Bank", "Start", "End", "Kind", "Detail")
$rangesByBank = @{}
foreach ($row in $rows) {
    foreach ($column in $requiredColumns) {
        if (-not ($row.PSObject.Properties.Name -contains $column)) {
            throw "Execution-seam manifest lacks '$column': $ManifestPath"
        }
    }
    $hasHexRange = ($row.Bank -match '^[0-9A-Fa-f]{2}$' -and $row.Start -match '^[0-9A-Fa-f]{4}$' -and $row.End -match '^[0-9A-Fa-f]{4}$')
    if (-not $hasHexRange) {
        throw "Execution seam has a non-hex range: $($row.Bank):$($row.Start)-$($row.End)"
    }
    if ($row.Kind -notin @("core", "host", "guarded")) {
        throw "Execution seam has unsupported kind '$($row.Kind)'."
    }
    if ([string]::IsNullOrWhiteSpace($row.Detail)) {
        throw "Execution seam lacks a reason: $($row.Bank):$($row.Start)-$($row.End)"
    }
    $bank = $row.Bank.ToUpperInvariant()
    $start = [Convert]::ToInt32($row.Start, 16)
    $end = [Convert]::ToInt32($row.End, 16)
    if ($start -gt $end) {
        throw "Execution seam start exceeds end: ${bank}:$($row.Start)-$($row.End)"
    }
    if (-not $rangesByBank.ContainsKey($bank)) {
        $rangesByBank[$bank] = [System.Collections.Generic.List[object]]::new()
    }
    $rangesByBank[$bank].Add([pscustomobject] @{ Start = $start; End = $end }) | Out-Null
}

foreach ($bank in $rangesByBank.Keys) {
    $previousEnd = -1
    foreach ($range in @($rangesByBank[$bank] | Sort-Object Start, End)) {
        if ($range.Start -le $previousEnd) {
            throw "Execution-seam manifest contains an overlapping range in bank $bank."
        }
        $previousEnd = $range.End
    }
}

Write-Output ("Native execution seams verified: {0} classified ranges." -f $rows.Count)
