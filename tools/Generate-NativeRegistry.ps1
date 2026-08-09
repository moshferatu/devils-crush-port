# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
    Derives the admitted native-routine registry from the ROM regression scripts.

.DESCRIPTION
    A native replacement is admissible for live play only when a regression has
    proven it for every naturally observed call at its address. That evidence is
    expressed today as a `--replace-hook <bank>:<pc>@all` invocation plus an
    expected `Native replacement <bank>:<pc>@all entries: <replaced> of
    <observed>` output line inside `tests/verify_*.cmake`.

    This script sweeps those scripts, cross-checks each address against the
    dispatch chain in `src/app/native_replacements.cpp` and the self-guarding
    `kRoutine` constants in the family sources, and emits the X-macro table
    consumed by `src/app/native_registry.cpp`.

    Addresses proven only for a single ordinal (`@3`) are deliberately excluded:
    "the third call" has no meaning outside a deterministic recorded route.

.PARAMETER Check
    Compare the freshly derived table against the checked-in file and fail on
    drift instead of rewriting it.
#>

[CmdletBinding()]
param(
    [string] $RepositoryRoot,
    [string] $TablePath,
    [string] $ReportPath,
    [switch] $Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $RepositoryRoot) {
    $RepositoryRoot = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
}
if (-not $TablePath) {
    $TablePath = Join-Path $RepositoryRoot "src/app/native_registry_table.inc"
}

$testsDirectory = Join-Path $RepositoryRoot "tests"
if (-not (Test-Path -LiteralPath $testsDirectory)) {
    throw "Regression directory not found: $testsDirectory"
}

# --- Admission evidence ------------------------------------------------------

# Shape 1: the address is written out literally in the expected output line.
$literalPattern = 'Native replacement (?<bank>[0-9A-Fa-f]{2}):(?<pc>[0-9A-Fa-f]{4})@all entries: (?<replaced>[\d,]+)(?: of (?<observed>[\d,]+))?'

# Shape 2: a helper function is called once per address, so the expected line
# interpolates the routine. Every quoted address literal in such a file is
# admitted with the counts asserted by that shared line.
$parameterizedPattern = 'Native replacement \$\{\w+\}@all entries: (?<replaced>[\d,]+)(?: of (?<observed>[\d,]+))?'
$addressLiteralPattern = '"(?<bank>[0-9A-Fa-f]{2}):(?<pc>[0-9A-Fa-f]{4})"'

function ConvertTo-Count {
    param([string] $Text)

    if ([string]::IsNullOrEmpty($Text)) {
        return [uint64] 0
    }
    return [uint64] ($Text -replace ",", "")
}

$admitted = [ordered] @{}

function Add-Admission {
    param(
        $Target,
        [string] $Address,
        [uint64] $Replaced,
        [uint64] $Observed,
        [string] $Evidence,
        [bool] $Patched
    )

    if ($Target.Contains($Address)) {
        $existing = $Target[$Address]
        if ($Observed -gt $existing.Observed) {
            $existing.Replaced = $Replaced
            $existing.Observed = $Observed
        }
        if (-not $Patched) {
            $existing.NaturalProof = $true
        }
        if ($existing.Evidence -notcontains $Evidence) {
            $existing.Evidence.Add($Evidence) | Out-Null
        }
        return
    }

    $evidenceList = [System.Collections.Generic.List[string]]::new()
    $evidenceList.Add($Evidence) | Out-Null
    $Target[$Address] = [pscustomobject] @{
        Address = $Address
        Replaced = $Replaced
        Observed = $Observed
        Evidence = $evidenceList
        NaturalProof = (-not $Patched)
    }
}

$scripts = @(Get-ChildItem -LiteralPath $testsDirectory -Filter "*.cmake" | Sort-Object Name)
foreach ($script in $scripts) {
    $text = Get-Content -LiteralPath $script.FullName -Raw
    $evidence = [System.IO.Path]::GetFileNameWithoutExtension($script.Name)

    # A route that patches work RAM proves the routine against a state the
    # regression forced rather than one the game reached. The proof is still
    # real, but it is recorded separately so a deployment decision can see how
    # the address was reached.
    $patched = $text.Contains("--work-ram-patch")
    $target = $admitted

    foreach ($match in [regex]::Matches($text, $literalPattern)) {
        $address = "{0}:{1}" -f
            $match.Groups["bank"].Value.ToUpperInvariant(),
            $match.Groups["pc"].Value.ToUpperInvariant()
        $replaced = ConvertTo-Count $match.Groups["replaced"].Value
        $observed = ConvertTo-Count $match.Groups["observed"].Value
        if ($observed -eq 0) {
            $observed = $replaced
        }
        Add-Admission -Target $target -Address $address -Replaced $replaced -Observed $observed -Evidence $evidence -Patched $patched
    }

    $parameterized = [regex]::Match($text, $parameterizedPattern)
    if (-not $parameterized.Success) {
        continue
    }
    $replaced = ConvertTo-Count $parameterized.Groups["replaced"].Value
    $observed = ConvertTo-Count $parameterized.Groups["observed"].Value
    if ($observed -eq 0) {
        $observed = $replaced
    }
    foreach ($match in [regex]::Matches($text, $addressLiteralPattern)) {
        $address = "{0}:{1}" -f
            $match.Groups["bank"].Value.ToUpperInvariant(),
            $match.Groups["pc"].Value.ToUpperInvariant()
        Add-Admission -Target $target -Address $address -Replaced $replaced -Observed $observed -Evidence $evidence -Patched $patched
    }
}

if ($admitted.Count -eq 0) {
    throw "No @all admissions found. The regression evidence format has changed."
}

# --- Dispatch reachability ---------------------------------------------------

$sourceDirectory = Join-Path $RepositoryRoot "src/app"
$chainSource = Get-Content -LiteralPath (Join-Path $sourceDirectory "native_replacements.cpp") -Raw
$chainPattern = 'entry\.routine\s*==\s*RoutineKey\{\s*\.physical_bank\s*=\s*0x(?<bank>[0-9A-Fa-f]{2})U?,\s*\.logical_pc\s*=\s*0x(?<pc>[0-9A-Fa-f]{4})U?,?\s*\}'
$guardPattern = 'constexpr RoutineKey kRoutine\{\s*\.physical_bank\s*=\s*0x(?<bank>[0-9A-Fa-f]{2})U?,\s*\.logical_pc\s*=\s*0x(?<pc>[0-9A-Fa-f]{4})U?,?\s*\}'

$dispatch = [System.Collections.Generic.HashSet[string]]::new()
foreach ($match in [regex]::Matches($chainSource, $chainPattern)) {
    $dispatch.Add(
        ("{0}:{1}" -f
            $match.Groups["bank"].Value.ToUpperInvariant(),
            $match.Groups["pc"].Value.ToUpperInvariant())) | Out-Null
}
foreach ($family in Get-ChildItem -LiteralPath $sourceDirectory -Filter "native_replacements*.cpp") {
    $familySource = Get-Content -LiteralPath $family.FullName -Raw
    foreach ($match in [regex]::Matches($familySource, $guardPattern)) {
        $dispatch.Add(
            ("{0}:{1}" -f
                $match.Groups["bank"].Value.ToUpperInvariant(),
                $match.Groups["pc"].Value.ToUpperInvariant())) | Out-Null
    }
}

$unreachable = @($admitted.Keys | Where-Object { -not $dispatch.Contains($_) })
if ($unreachable.Count -ne 0) {
    throw ("Admitted addresses are not reachable in the dispatch chain: " +
        ($unreachable -join ", "))
}

$unproven = @($dispatch | Where-Object { -not $admitted.Contains($_) } | Sort-Object)

# --- Emit --------------------------------------------------------------------

$rows = @($admitted.Values | Sort-Object Address)
$builder = [System.Text.StringBuilder]::new()
[void] $builder.Append(@"
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Generated by tools/Generate-NativeRegistry.ps1. Do not edit by hand.
//
// One row per routine whose native replacement has been admitted for every
// naturally observed call at its address. The counts are the replaced and
// observed call totals asserted by the named ROM regression; a shortfall means
// the remaining calls fall back to emulated assembly by design.


"@.Replace("`r`n", "`n"))

foreach ($row in $rows) {
    $parts = $row.Address.Split(":")
    $sorted = @($row.Evidence | Sort-Object)
    $evidence = $sorted[0]
    if ($sorted.Count -gt 1) {
        $evidence += " +{0}" -f ($sorted.Count - 1)
    }
    [void] $builder.Append(
        ("DC_ADMITTED_ROUTINE(0x{0}U, 0x{1}U, {2}, {3}, `"{4}`")`n" -f
            $parts[0], $parts[1], $row.Replaced, $row.Observed, $evidence))
}

$table = $builder.ToString()

$totalReplaced = ($rows | Measure-Object -Property Replaced -Sum).Sum
$totalObserved = ($rows | Measure-Object -Property Observed -Sum).Sum
$partial = @($rows | Where-Object { $_.Replaced -ne $_.Observed })

if ($Check) {
    if (-not (Test-Path -LiteralPath $TablePath)) {
        throw "Registry table is missing: $TablePath"
    }
    $current = [System.IO.File]::ReadAllText($TablePath).Replace("`r`n", "`n")
    if ($current -ne $table) {
        throw ("Registry table is stale. The regression evidence no longer " +
            "matches $TablePath. Re-run tools/Generate-NativeRegistry.ps1.")
    }
    Write-Host "Native registry is current: $($rows.Count) admitted routines."
} else {
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($TablePath, $table, $encoding)
    Write-Host "Wrote $TablePath"
}

if ($ReportPath) {
    $lines = [System.Collections.Generic.List[string]]::new()
    $percent = "{0:N1}" -f (100.0 * $totalReplaced / $totalObserved)

    $lines.Add('# Native Registry')
    $lines.Add('')
    $lines.Add('Generated by `tools/Generate-NativeRegistry.ps1`. Do not edit by hand.')
    $lines.Add('')
    $lines.Add("$($rows.Count) routines are admitted for every naturally observed call at")
    $lines.Add("their address. Across those routes $('{0:N0}' -f $totalReplaced) of $('{0:N0}' -f $totalObserved) observed")
    $lines.Add("calls ($percent%) were proven native; the remainder fall back to emulated")
    $lines.Add('assembly by design, because the dispatcher returns no replacement for a')
    $lines.Add('state it has not been proven against.')
    $lines.Add('')
    $lines.Add('## Measured cycle share')
    $lines.Add('')
    $lines.Add('Deploying the registry with `--replace-registry` over the 3,600-frame')
    $lines.Add('Release state-census route (`--native-cycle-census`, measured 2026-08-08):')
    $lines.Add('')
    $lines.Add('| | |')
    $lines.Add('| --- | ---: |')
    $lines.Add('| Elapsed master cycles | 1,290,049,560 |')
    $lines.Add('| Native master cycles | 467,299,485 |')
    $lines.Add('| **Native share** | **36.22%** |')
    $lines.Add('| Routines installed | 282 of 284 |')
    $lines.Add('| Routines entered | 215 |')
    $lines.Add('| Calls run natively | 2,159,816 of 2,187,361 entered |')
    $lines.Add('| Calls declined at an interrupt horizon | 3,599 |')
    $lines.Add('')
    $lines.Add('The resulting state census is byte-identical to the assembly-only')
    $lines.Add('baseline (`9036a9de…`), so that 36.22% is faithful, not merely running.')
    $lines.Add('Two admitted routines are withheld from that run because each changes the')
    $lines.Add('census on its own — `02:57E9` and `03:6054`. Both are proven `@all` on an')
    $lines.Add('unpatched route, but their regressions assert a census hash that differs')
    $lines.Add('from the baseline, so the change was blessed per-routine and is inherited')
    $lines.Add('by any run that deploys them. Adding them back with explicit hooks still')
    $lines.Add('completes 3,600 frames, at a census (`fe61e05b…`) that is not the')
    $lines.Add('assembly one.')
    $lines.Add('')
    $lines.Add('The hold-back is not a flag on the measurement command: it lives in')
    $lines.Add('`kWithheldRoutines` in `src/app/main.cpp` and applies to every composed')
    $lines.Add('deployment, so the interactive host runs the same faithful 282 that this')
    $lines.Add('measurement does. Each run names them in its deployment summary.')
    $lines.Add('')
    $lines.Add('## Cycle budget')
    $lines.Add('')
    $lines.Add('A whole-ROM `--instruction-profile` of the same deployed route (7,443')
    $lines.Add('executed addresses, 1,292,378,139 master cycles) shows what the')
    $lines.Add('remainder is:')
    $lines.Add('')
    $lines.Add('| Category | Master cycles | Share of all cycles | Share of working cycles |')
    $lines.Add('| --- | ---: | ---: | ---: |')
    $lines.Add('| Frame-sync spin loop (`00:E2FB`–`E39C`) | 584,757,771 | 45.25% | — |')
    $lines.Add('| Timer sound driver (`00:FC00`–`FE48`) | 177,203,574 | 13.71% | 25.12% |')
    $lines.Add('| Transcripts resumed after its RTI | 72,383,913 | 5.60% | — |')
    $lines.Add('| Music sequencer (bank `45`) | 46,078,773 | 3.57% | 6.53% |')
    $lines.Add('| Video banks (`08`, `09`) | 96,288,942 | 7.45% | 13.65% |')
    $lines.Add('| Gameplay pool | 315,665,166 | 24.42% | 44.74% |')
    $lines.Add('')
    $lines.Add('Nearly half the machine is idle: `00:E323` is `CMP $00` / `BEQ -4`,')
    $lines.Add('spinning on the VBlank counter 24.2 million times. That is the game')
    $lines.Add('waiting, not working, so it is not a porting target and it is the slack')
    $lines.Add('that absorbs anything the port makes faster.')
    $lines.Add('')
    $lines.Add('The second row needs the third to read honestly. The profile charges each')
    $lines.Add('address the span to the next instruction hook, and the driver ends in an')
    $lines.Add('`RTI` that returns into whatever the timer interrupted. When that is a')
    $lines.Add('suspended checkpointed transcript, the rest of it replays with no hook in')
    $lines.Add('between, so those cycles land on the driver''s `RTI`: `00:FC8D` shows 305')
    $lines.Add('master cycles per execution against a real cost of 21. Those 72,383,913')
    $lines.Add('cycles are other routines'' native bodies, already counted as native, not')
    $lines.Add('sound-driver work. Every other address in the driver profiles at a clean')
    $lines.Add('6, 12, 15, 18 or 21.')
    $lines.Add('')
    $lines.Add('## The sound driver')
    $lines.Add('')
    $lines.Add('The timer interrupt handler is the sound driver, and it now runs')
    $lines.Add('natively. It is a flat state machine over `00:FC00`–`00:FE48` with no')
    $lines.Add('subroutine calls at all: three PSG channels walked at X = 4, 2 and 0,')
    $lines.Add('each streaming five-bit DDA samples out of a sequence bank mapped into')
    $lines.Add('MPR6 with `TAM #$40` and pushed through the PSG waveform port at')
    $lines.Add('`$0806`. The eight banks it selects between are listed in ROM at')
    $lines.Add('`$FC06`, immediately ahead of the handler''s first instruction.')
    $lines.Add('')
    $lines.Add('Earlier reports placed bank `45` here and called it the PSG driver. That')
    $lines.Add('was wrong. `$FC06` holds `48 49 4A 4B 4C 4D 4E 4F`, so the driver''s data')
    $lines.Add('banks are `48`-`4F`, mapped at `$C000` and only ever read. Bank `45`')
    $lines.Add('runs at `$4000`-`$4CB8` and is the music sequencer, a separate component')
    $lines.Add('that programmes the PSG registers `$0801`-`$0809` directly and is not')
    $lines.Add('yet ported.')
    $lines.Add('')
    $lines.Add('Taking it needed the transaction write channel widened past work RAM and')
    $lines.Add('the four VDC ports. It now also carries the PSG register file')
    $lines.Add('`$0800`-`$0809`, the timer at `$0C00`-`$0C01`, and the interrupt')
    $lines.Add('controller at `$1402`-`$1403`. Each is a whole register file on the')
    $lines.Add('hardware page whose writes are ordinary stores with no wait state, so a')
    $lines.Add('transcript prices them like any other instruction — unlike the VDC')
    $lines.Add('ports, which cost three extra cycles. Because device writes are')
    $lines.Add('invisible to a work-RAM census, `--bus-write-range` and')
    $lines.Add('`--bus-write-trace` were added to census them; the driver''s regression')
    $lines.Add('asserts all 954,241 PSG, timer and interrupt-acknowledge writes on the')
    $lines.Add('route, byte-identical to assembly.')
    $lines.Add('')
    $lines.Add('Two things bound what the transcript may cover. From the `CLI` at')
    $lines.Add('`$FC32` the handler is interruptible, and a transcript is computed once')
    $lines.Add('from one snapshot — so it cannot observe a write a preempting handler')
    $lines.Add('makes to memory it later reads. The epilogue reads `$DA`, the rerun')
    $lines.Add('request such a handler sets, and the walk''s own `STZ $DA` at `$FC30`')
    $lines.Add('would make that read always zero, so the handler would never take its')
    $lines.Add('`JMP $FC03` chain exit into the sequencer. The walk therefore stops at')
    $lines.Add('the two epilogue heads `$FC7C` and `$FD70`, plus `$FC8C` for a')
    $lines.Add('reentrant interrupt that bails without touching a channel, and leaves')
    $lines.Add('the epilogue emulated so that read stays live.')
    $lines.Add('')
    $lines.Add('The other is `CLI` itself. The core samples the interrupt on an')
    $lines.Add('instruction''s last cycle, and `OP_CLEARF` runs that sample before it')
    $lines.Add('clears the flag, so the instruction after a `CLI` still runs masked. A')
    $lines.Add('checkpoint that published the post-`CLI` status opened the window one')
    $lines.Add('instruction early and a nested timer interrupt arrived nine master')
    $lines.Add('cycles ahead of the machine. The `CLI` checkpoint must publish the old')
    $lines.Add('mask and let the clear land on the next one.')
    $lines.Add('')
    $lines.Add('Hooked on its own the driver is admitted at every entry — 254,434 of')
    $lines.Add('254,434. With the whole registry deployed it is admitted 132,656 times')
    $lines.Add('of 254,990, 52.0%. The rest arrive while another checkpointed transcript')
    $lines.Add('is suspended, and the core does not consult the replacement hook while')
    $lines.Add('one is active, so those run emulated. That is the single largest')
    $lines.Add('remaining lever in the registry: roughly 70,000,000 cycles are blocked')
    $lines.Add('by the no-nesting rule rather than by anything unported.')
    $lines.Add('')
    $lines.Add('## What is left')
    $lines.Add('')
    $lines.Add('The portable pool has grown, because sound is now in scope. It is the')
    $lines.Add('315,665,166-cycle gameplay pool, the three bank `08` satellite paths')
    $lines.Add('(128,745,675), the timer sound driver (177,203,574) and the music')
    $lines.Add('sequencer (46,078,773) — 667,693,188 in all. Against that the')
    $lines.Add('467,299,485 native cycles are **70.0%**. That figure fell from the 81.4%')
    $lines.Add('reported when the pool excluded sound, not because less runs natively')
    $lines.Add('but because more of the machine is now counted as portable.')
    $lines.Add('')
    $lines.Add('Set the idle spin loop aside and 707,620,368 cycles are the machine')
    $lines.Add('working; 467,299,485 of them, 66.0%, now run natively. The 240,320,883')
    $lines.Add('still emulated are three blocks:')
    $lines.Add('')
    $lines.Add('| Still emulated outside the spin loop | Master cycles |')
    $lines.Add('| --- | ---: |')
    $lines.Add('| Sound-driver entries the no-nesting rule blocks | 102,153,144 |')
    $lines.Add('| Music sequencer (bank `45`) | 46,078,773 |')
    $lines.Add('| Remaining video-bank and gameplay code | 92,088,966 |')
    $lines.Add('')
    $lines.Add('The first of those is not unported code. It is the 48% of driver entries')
    $lines.Add('that arrive while another checkpointed transcript is suspended, which the')
    $lines.Add('core runs emulated because it will not nest a replacement. The second is')
    $lines.Add('the largest genuinely unported block left. The third is spread thin: no')
    $lines.Add('single address in it is worth more than 0.14% of the machine.')
    $lines.Add('')
    $lines.Add('Five whole loops are now native instruction for instruction: the main')
    $lines.Add('actor loop (`01:C510`, `01:C51E`, `01:C520`, `01:C523`, `01:C52C`,')
    $lines.Add('`01:C606`, `01:C654`), the collision sweep (`01:CB48`, `01:CB61`,')
    $lines.Add('`01:CB6B`), the camera-visibility sweep (`00:F4F2`, `00:F4FC`, `00:F523`,')
    $lines.Add('`00:F545`, `00:F54E`), the SAT submission (`08:4036`, `08:4063`,')
    $lines.Add('`08:4075`, `08:4088`, `08:4097`, `08:40C3`, `08:40DF`), and the timer')
    $lines.Add('sound driver (`00:FC00`).')
    $lines.Add('')
    $lines.Add('Cycle share is concentrated. The timer sound driver `00:FC00` is now')
    $lines.Add('the single largest native address at 5.91% of all master cycles, ahead')
    $lines.Add('of `08:40DF` at 5.39% and `01:CC1A` (collision vertical scan) at')
    $lines.Add('4.43%; the top five together are 19.95% of the machine, more than half')
    $lines.Add('of the 36.22% that runs natively. Most installed routines are never')
    $lines.Add('entered on this route, and many that are entered never accept a call.')
    $lines.Add('')
    $lines.Add('| Address | Native calls | Observed calls | Evidence |')
    $lines.Add('| --- | ---: | ---: | --- |')
    foreach ($row in $rows) {
        $evidence = ($row.Evidence | Sort-Object | ForEach-Object { '`' + $_ + '`' }) -join ', '
        if (-not $row.NaturalProof) {
            $evidence += ' (patched route)'
        }
        $lines.Add(
            "| ``$($row.Address)`` | $('{0:N0}' -f $row.Replaced) | " +
            "$('{0:N0}' -f $row.Observed) | $evidence |")
    }
    $lines.Add('')
    $lines.Add('## Dispatch-reachable without `@all` evidence')
    $lines.Add('')
    if ($unproven.Count -eq 0) {
        $lines.Add('None.')
    } else {
        $lines.Add('These addresses reach a handler but no regression proves them for every')
        $lines.Add('call, so they stay out of the registry:')
        $lines.Add('')
        foreach ($address in $unproven) {
            $lines.Add('- `' + $address + '`')
        }
    }

    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText(
        $ReportPath,
        (($lines -join "`n") + "`n"),
        $encoding)
    Write-Host "Wrote $ReportPath"
}

Write-Host ("Admitted routines        : {0}" -f $rows.Count)
Write-Host ("Proven on a patched route: {0}" -f
    @($rows | Where-Object { -not $_.NaturalProof }).Count)
Write-Host ("Dispatch-reachable keys  : {0}" -f $dispatch.Count)
Write-Host ("Proven calls             : {0:N0} of {1:N0} ({2:N1}%)" -f
    $totalReplaced, $totalObserved, (100.0 * $totalReplaced / $totalObserved))
Write-Host ("Partial-coverage routes  : {0}" -f $partial.Count)
Write-Host ("Reachable without @all   : {0}{1}" -f
    $unproven.Count, $(if ($unproven.Count) { " ($($unproven -join ', '))" } else { "" }))
