# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ManifestPath,

    [Parameter(Mandatory = $true)]
    [string] $TestListPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$manifest = Get-Content -LiteralPath $ManifestPath
$rows = [System.Collections.Generic.List[object]]::new()
foreach ($line in $manifest) {
    if ($line -match '^\| `(?<id>[^`]+)` \|.*\| (?<class>[DSF]) \|') {
        $rows.Add(
            [pscustomobject] @{
                Id = $Matches.id
                Class = $Matches.class
            }
        )
    }
}

if ($rows.Count -ne 105) {
    throw "Expected 105 classified coverage rows, found $($rows.Count)."
}
$duplicates = @(
    $rows |
        Group-Object Id |
        Where-Object Count -ne 1
)
if ($duplicates.Count -ne 0) {
    throw "Coverage IDs must be unique: $($duplicates.Name -join ',')."
}

$expectedFrontEnd = 1..19 | ForEach-Object { "M-FE-{0:D2}" -f $_ }
$expectedScoring = 1..9 | ForEach-Object { "M-SC-{0:D2}" -f $_ }
$expectedTable = @(
    "M-T01-A", "M-T01-B",
    "M-T02-04-A", "M-T02-04-B",
    "M-T05-A", "M-T05-B",
    "M-T06-A", "M-T06-B",
    "M-T07-A", "M-T07-B",
    "M-T08A-A", "M-T08A-B", "M-T08B",
    "M-T09A-A", "M-T09A-B", "M-T09B-A", "M-T09B-B",
    "M-T10-A", "M-T10-B",
    "M-T11A-A", "M-T11A-B", "M-T11B",
    "M-T12-14-A", "M-T12-14-B",
    "M-T15-A", "M-T15-B",
    "M-T16", "M-T17",
    "M-T18-A", "M-T18-B",
    "M-T19-A", "M-T19-B",
    "M-T20-A", "M-T20-B",
    "M-T21-A", "M-T21-B",
    "M-T22", "M-T23",
    "M-T24-27-A", "M-T24-27-B",
    "M-T28-A", "M-T28-B"
)
$expectedBonus = 1..8 | ForEach-Object { "M-BS-{0:D2}" -f $_ }
$requiredManualIds =
    @($expectedFrontEnd) +
    @($expectedScoring) +
    @($expectedTable) +
    @($expectedBonus)

$actualIds = @($rows.Id)
$missing = @($requiredManualIds | Where-Object { $_ -notin $actualIds })
if ($missing.Count -ne 0) {
    throw "Manual coverage IDs are missing: $($missing -join ',')."
}

$dynamicCount = @($rows | Where-Object Class -eq "D").Count
$staticCount = @($rows | Where-Object Class -eq "S").Count
$deferredCount = @($rows | Where-Object Class -eq "F").Count
if (
    $dynamicCount -ne 58 -or
    $staticCount -ne 28 -or
    $deferredCount -ne 19
) {
    throw (
        "Coverage classification totals changed: " +
        "D=$dynamicCount S=$staticCount F=$deferredCount."
    )
}

$testList = Get-Content -Raw -LiteralPath $TestListPath
$evidenceTests = [System.Collections.Generic.HashSet[string]]::new()
foreach ($line in $manifest) {
    foreach ($match in [regex]::Matches($line, 'dc_rom_[a-z0-9_]+')) {
        [void] $evidenceTests.Add($match.Value)
    }
}
$missingTests = @(
    $evidenceTests |
        Where-Object {
            $testList -notmatch (
                '(?m)^\s*NAME\s+' + [regex]::Escape($_) + '\s*$'
            )
        } |
        Sort-Object
)
if ($missingTests.Count -ne 0) {
    throw "Manifest evidence tests are not registered: $($missingTests -join ',')."
}

$requiredAuditPhrases = @(
    "Play Mode, Speed, New Game, Password, Data Load",
    "Controls, pause, reset, save/password access",
    "Last Chance, game over, High Scores/name entry",
    "Score display, BP/BT/BM, Bonus Balls",
    "Numbered playfield chart 1-28",
    "Backup-memory save flow and five files",
    "Six bonus stages reached by the chart's entrances",
    "999,999,900 devil-destruction ending"
)
$manifestText = $manifest -join "`n"
foreach ($phrase in $requiredAuditPhrases) {
    if (-not $manifestText.Contains($phrase)) {
        throw "Completeness audit omitted '$phrase'."
    }
}

Write-Output "Coverage manifest verified."
Write-Output (
    "Classified rows: 105 total; 58 dynamic, 28 static, 19 deferred."
)
Write-Output (
    "Manual audit: frontend, controls, session, scoring, chart 1-28, " +
    "backup memory, six bonus stages, and maximum ending are classified."
)
