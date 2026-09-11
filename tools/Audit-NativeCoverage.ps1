# Compare existing diagnostic counters; Unreconciled is not an ownership class.
# No runtime execution or mutation of the input captures is performed here.
param(
    [string] $InputDirectory = 'out/ranked-vdc-cost',
    [string] $OutputDirectory = 'out/coverage-gap-audit',
    [string] $RomPath = "Devil's Crush.pce"
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$rom = [IO.File]::ReadAllBytes((Resolve-Path $RomPath))
if ((Get-FileHash $RomPath -Algorithm SHA256).Hash -ne '9ad122903ec34cb8e62ba1aae1cfddbfcb06657d0fba55d6d0b1d280f88e30f9') {
    throw 'Opcode grouping requires the supported Devil''s Crush USA ROM'
}
$sites = @{}
$routes = @(foreach ($log in Get-ChildItem $InputDirectory -Filter '*.after.log') {
    $route = $log.Name.Replace('.after.log', '')
    $content = [IO.File]::ReadAllText($log.FullName)
    $share = [regex]::Match($content, 'Registry cycles outside the frame-sync wait: (\d+) native of (\d+)')
    $residue = [regex]::Match($content, 'Interpreter residue outside the frame-sync wait: (\d+) executed instruction interval\(s\), (\d+) master cycles')
    if (!$share.Success -or !$residue.Success) { throw "Missing counters: $route" }
    $native = [long]$share.Groups[1].Value
    $working = [long]$share.Groups[2].Value
    $fallback = [long]$residue.Groups[2].Value
    $capabilities = [regex]::Match($content, 'Registry core capabilities: (\d+) native core instruction\(s\) of \d+ entered \((\d+) master cycles')
    if (!$capabilities.Success) { throw "Missing capability counters: $route" }
    $profile = Join-Path $InputDirectory "$route.after.profile.csv.interpreter.csv"
    $rows = @(Get-Content $profile | Where-Object { !$_.StartsWith('#') } | ConvertFrom-Csv)
    [long]$sum = 0
    foreach ($row in $rows) {
        $bank = [Convert]::ToInt32($row.physical_bank, 16)
        $pc = [Convert]::ToInt32($row.logical_pc, 16)
        if ($bank -eq 0 -and $pc -ge 0xE321 -and $pc -le 0xE32B) { continue }
        $key = "$($row.physical_bank):$($row.logical_pc)"
        $cycles = [long]$row.master_cycles
        $sum += $cycles
        if (!$sites.ContainsKey($key)) {
            $opcode = 'unmapped'
            if ($bank -lt 0x80) {
                $mapped = if ($bank -lt 0x40) { $bank -band 0x1F } else { ($bank -band 0xF) + 0x20 }
                $opcode = '{0:X2}' -f $rom[$mapped * 8192 + ($pc -band 8191)]
            }
            $sites[$key] = [pscustomobject]@{ Site=$key; Opcode=$opcode; Cycles=[long]0; Intervals=[long]0; Routes=0 }
        }
        $sites[$key].Cycles += $cycles
        $sites[$key].Intervals += [long]$row.executions
        $sites[$key].Routes++
    }
    if ($sum -ne $fallback) { throw "Residue profile/log mismatch: $route" }
    [pscustomobject]@{
        Route=$route; Native=$native; Working=$working; Gap=($working-$native)
        Residue=$fallback; Unreconciled=($working-$native-$fallback)
        CapabilityCredits=[long]$capabilities.Groups[2].Value
        ReportedShare=(100.0*$native/$working)
        ResiduePercent=(100.0*$fallback/$working)
    }
})
if (!$routes.Count) { throw 'No route logs found' }
$routes | Export-Csv (Join-Path $OutputDirectory 'routes.csv') -NoTypeInformation
$ranked = @($sites.Values | Sort-Object Cycles -Descending)
$ranked | Export-Csv (Join-Path $OutputDirectory 'sites.csv') -NoTypeInformation
$families = @($ranked | Group-Object Opcode | ForEach-Object {
    [pscustomobject]@{ Opcode=$_.Name; Sites=$_.Count; Cycles=[long](($_.Group | Measure-Object Cycles -Sum).Sum); Intervals=[long](($_.Group | Measure-Object Intervals -Sum).Sum) }
} | Sort-Object Cycles -Descending)
$families | Export-Csv (Join-Path $OutputDirectory 'opcodes.csv') -NoTypeInformation
$totals = [ordered]@{ Routes=$routes.Count; Sites=$ranked.Count }
foreach ($field in @('Native','Working','Gap','Residue','Unreconciled','CapabilityCredits')) {
    $totals[$field] = [long](($routes | Measure-Object $field -Sum).Sum)
}
$totals['WeightedShare'] = 100.0*$totals.Native/$totals.Working
$totals['ResiduePercent'] = 100.0*$totals.Residue/$totals.Working
$totals['ResidueFractionOfGap'] = 100.0*$totals.Residue/$totals.Gap
$totals | ConvertTo-Json | Set-Content (Join-Path $OutputDirectory 'totals.json')
$totals | ConvertTo-Json
$families | Select-Object -First 15 | Format-Table
$ranked | Select-Object -First 20 | Format-Table
