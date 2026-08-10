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
    $lines.Add('Release state-census route (`--native-cycle-census`, measured 2026-08-09):')
    $lines.Add('')
    $lines.Add('| | |')
    $lines.Add('| --- | ---: |')
    $lines.Add('| Elapsed master cycles | 1,292,371,131 |')
    $lines.Add('| Native master cycles | 521,505,306 |')
    $lines.Add('| **Native share** | **40.35%** |')
    $lines.Add('| Routines installed | 327 of 329 |')
    $lines.Add('| Routines entered | 260 |')
    $lines.Add('| Calls run natively | 2,351,585 of 2,381,008 entered |')
    $lines.Add('| Calls declined at an interrupt horizon | 3,599 |')
    $lines.Add('')
    $lines.Add('The resulting state census is byte-identical to the assembly-only')
    $lines.Add('baseline (`9036a9de…`), so that 40.35% is faithful, not merely running.')
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
    $lines.Add('deployment, so the interactive host runs the same faithful 327 that this')
    $lines.Add('measurement does. Each run names them in its deployment summary.')
    $lines.Add('')
    $lines.Add('## Cycle budget')
    $lines.Add('')
    $lines.Add('A whole-ROM `--instruction-profile` of the same deployed route (7,090')
    $lines.Add('executed addresses, 1,292,378,139 master cycles) shows what the')
    $lines.Add('remainder is:')
    $lines.Add('')
    $lines.Add('| Category | Master cycles | Share of all cycles | Share of working cycles |')
    $lines.Add('| --- | ---: | ---: | ---: |')
    $lines.Add('| Frame-sync spin loop (`00:E2FB`–`E39C`) | 584,757,771 | 45.25% | — |')
    $lines.Add('| Timer sound driver (`00:FC00`–`FE48`) | 172,648,227 | 13.36% | 24.40% |')
    $lines.Add('| Transcripts resumed after its RTI | 79,426,167 | 6.15% | 11.22% |')
    $lines.Add('| Music sequencer (bank `45`) | 45,473,625 | 3.52% | 6.43% |')
    $lines.Add('| Video banks (`08`, `09`) | 96,288,942 | 7.45% | 13.61% |')
    $lines.Add('| Gameplay pool | 313,783,407 | 24.28% | 44.34% |')
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
    $lines.Add('between, so those cycles land on the driver''s `RTI`: `00:FC8D` shows 315')
    $lines.Add('master cycles per execution against a real cost of 21. Those 79,233,825')
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
    $lines.Add('that programmes the PSG registers `$0801`-`$0809` directly. It has its')
    $lines.Add('own section below.')
    $lines.Add('')
    $lines.Add('Taking it needed the transaction write channel widened past work RAM and')
    $lines.Add('the four VDC ports. It now also carries the PSG register file')
    $lines.Add('`$0800`-`$0809`, the timer at `$0C00`-`$0C01`, and the interrupt')
    $lines.Add('controller at `$1402`-`$1403`. Each is a whole register file on the')
    $lines.Add('hardware page whose writes are ordinary stores with no wait state, so a')
    $lines.Add('transcript prices them like any other instruction — unlike the VDC')
    $lines.Add('ports, which cost three extra cycles. Device writes are invisible to')
    $lines.Add('a work-RAM census, so `--bus-write-range` and `--bus-write-trace` were')
    $lines.Add('added to census them directly. The state census would catch a wrong')
    $lines.Add('value on its own, since it hashes the audio as well as work RAM, but')
    $lines.Add('only as a changed hash somewhere in 3,600 frames. The regression')
    $lines.Add('asserts all 954,241 PSG, timer and interrupt-acknowledge writes,')
    $lines.Add('byte-identical to assembly.')
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
    $lines.Add('254,434. With the whole registry deployed it is admitted 122,457 times')
    $lines.Add('of 254,990, 48.0%. The rest arrive while another checkpointed transcript')
    $lines.Add('is suspended, and the core does not consult the replacement hook while')
    $lines.Add('one is active, so those run emulated. That is the single largest')
    $lines.Add('remaining lever on the percentage: 103,317,306 cycles are blocked by')
    $lines.Add('the no-nesting rule rather than by anything unported. It is a limit of')
    $lines.Add('the hybrid harness, not of the port, and it disappears with the')
    $lines.Add('emulator rather than before it.')
    $lines.Add('')
    $lines.Add('That figure moves the wrong way as the port grows, and the palette')
    $lines.Add('slice below is the first place it showed. A long transcript shadows the')
    $lines.Add('driver for its whole body: `00:E9B6` runs 9,745 cycles, spans about two')
    $lines.Add('timer periods, and the interrupts that land inside it cannot reach the')
    $lines.Add('driver''s hook. Making the three palette leaves native added 9,937,182')
    $lines.Add('cycles and gave back 694,000 of them in driver entries — 1,181 fewer')
    $lines.Add('than before. Net it is still strongly positive, but the arithmetic is')
    $lines.Add('worth stating: the longer the transcripts get, the more of the driver')
    $lines.Add('the no-nesting rule swallows.')
    $lines.Add('')
    $lines.Add('The exception is worth as much as the rule. `00:EB30`, added in the')
    $lines.Add('same area a slice later, runs up to 18,585 cycles — more than three')
    $lines.Add('timer periods — and cost the driver nothing at all: not one entry, not')
    $lines.Add('one cycle. Every one of its 3,599 calls arrives with `I` already set,')
    $lines.Add('because the upload has to land inside the blanking interval, so no')
    $lines.Add('timer interrupt was ever going to be delivered inside it. Body length')
    $lines.Add('is only half the question; whether the routine runs masked is the')
    $lines.Add('other half, and it is one `--observe-hook` run to find out.')
    $lines.Add('')
    $lines.Add('The wrapper slice below then settled what the cost actually scales with,')
    $lines.Add('and it is not per-call length. Its nine transcripts are 36 to 162 cycles')
    $lines.Add('each — a fortieth of `00:E9B6` — and there are 52,911 of them, 4,849,479')
    $lines.Add('cycles of native time in total. They cost the driver 1,326 entries and')
    $lines.Add('756,342 cycles, 15.6% of the slice''s own gross, which was the largest')
    $lines.Add('shadowing bill any slice had paid until the tilemap upload below. Total')
    $lines.Add('native time is the quantity that sets the scale, not per-call length: a')
    $lines.Add('thousand small transcripts shadow the driver exactly as well as one large')
    $lines.Add('one. The split inside the slice is the proof: its three masked wrappers run')
    $lines.Add('inside the VBlank handler and cost nothing at all, and the whole bill comes')
    $lines.Add('from the six that run on the main thread.')
    $lines.Add('')
    $lines.Add('## The music sequencer')
    $lines.Add('')
    $lines.Add('Bank `45` is the other half of the audio path. The timer driver streams')
    $lines.Add('samples; the sequencer decides what they are, walking its voices once per')
    $lines.Add('frame from `45:4015`, programming the PSG register file and maintaining')
    $lines.Add('the per-voice records in `$2E00`-`$2FFF`.')
    $lines.Add('')
    $lines.Add('Unlike the driver it is not flat: the tick calls sixteen subroutines, so')
    $lines.Add('the port is incremental rather than one transcript. Ten of them are now')
    $lines.Add('native — the sample/envelope group the voice walk calls once per voice per')
    $lines.Add('frame, the register programmer that pushes the result out, and the two')
    $lines.Add('routines that decide what the notes are in the first place:')
    $lines.Add('')
    $lines.Add('| Entry | Admitted | What it does |')
    $lines.Add('| --- | ---: | --- |')
    $lines.Add('| `45:48B4` | 72 of 72 | Sequence-data fetch, the shared command decoder |')
    $lines.Add('| `45:4883` | 22,664 of 22,664 | Pitch/period update (inlines `48B4`) |')
    $lines.Add('| `45:48F6` | 22,664 of 22,664 | Sweep accumulator update (inlines `48B4`) |')
    $lines.Add('| `45:49A2` | 22,664 of 22,664 | Vibrato accumulator update |')
    $lines.Add('| `45:49B6` | 3,604 of 3,604 | Voice-latch clear |')
    $lines.Add('| `45:49F9` | 22,622 of 22,664 | Voice commit, which drives `$0800`/`$0804` |')
    $lines.Add('| `45:4211` | 7,198 of 7,198 | The two pending-event gates |')
    $lines.Add('| `45:4228` | 1,142 of 1,142 | Portamento stepper |')
    $lines.Add('| `45:457A` | 2,219 of 2,299 | Command interpreter |')
    $lines.Add('| `45:4142` | 1,792 of 3,604 | PSG register programmer, six voices plus the LFO |')
    $lines.Add('')
    $lines.Add('One walk serves all ten. Each stops in front of its own `RTS` and hands')
    $lines.Add('the return back to the core, and any call the walk makes on the way is')
    $lines.Add('inlined into the same transcript — which is why `48B4` is admitted only')
    $lines.Add('72 times on its own: the other 25,896 calls to it are made from inside a')
    $lines.Add('`4883` or `48F6` transcript and never reach the hook. `457A` inlines four')
    $lines.Add('callees the same way: the `$42BA` dispatch trampoline at `4569`, the')
    $lines.Add('balance ramp `4522`, the note-to-period conversion `46D7`, and the forty')
    $lines.Add('command handlers themselves.')
    $lines.Add('')
    $lines.Add('Three rows are partial, for two different reasons.')
    $lines.Add('')
    $lines.Add('`$F5`, `$F7` and `$F9` are the timer driver''s per-channel bank bytes, and')
    $lines.Add('they are the only bytes in this bank''s working set that another thread of')
    $lines.Add('control writes: the driver sets one when it maps a channel''s data bank and')
    $lines.Add('clears it when the channel falls idle. `4142` reads all three, `49F9`')
    $lines.Add('reaches them through `$20F5,Y`, and a transcript reads them from a snapshot')
    $lines.Add('taken before it runs. The walk therefore declines unless the interrupt mask')
    $lines.Add('is already set at entry — `4142` brackets each voice in PHP/SEI/PLP, so a')
    $lines.Add('masked entry stays masked for the whole body — or the body finishes before')
    $lines.Add('the next serviceable timer interrupt. That is a correctness boundary, not a')
    $lines.Add('horizon heuristic: without it the transcript would act on a value the driver')
    $lines.Add('had already changed.')
    $lines.Add('')
    $lines.Add('`457A` declines for a different reason: 45 of its calls hit the `$82`/`$92`')
    $lines.Add('command, whose handler pops four bytes off the stack and jumps to `$40EE`,')
    $lines.Add('abandoning the routine''s own frame. A transcript that has to stop in front')
    $lines.Add('of an `RTS` cannot express that. The other 35 are command handlers the')
    $lines.Add('census route never reaches, left untranscribed rather than written blind.')
    $lines.Add('Both would go away if `45:4015` were ported, because at that level the')
    $lines.Add('escape is ordinary internal control flow — but it cannot be, for the')
    $lines.Add('reason below.')
    $lines.Add('')
    $lines.Add('Verification goes past the state census here, and it earned its keep twice.')
    $lines.Add('The census is the direct oracle for an audio port, because it hashes the')
    $lines.Add('audio, and it is unchanged. On top of that the regression compares three')
    $lines.Add('artifacts against a pure-assembly run of the same route: 5,458,568 work-RAM')
    $lines.Add('writes covering every voice record, the zero-page command pointer, the')
    $lines.Add('timer driver''s per-channel state and the stack, 699,251 PSG and timer')
    $lines.Add('writes, and the census itself. All three are byte-identical, so the writes')
    $lines.Add('match in value, address, order and master-cycle offset, not merely in end')
    $lines.Add('state.')
    $lines.Add('')
    $lines.Add('The stack range is the load-bearing part, and it has now caught two faults')
    $lines.Add('the census could not. `JSR` pushes the return address *before* `ADDCYC(6)`,')
    $lines.Add('while every other store in the walk lands on its instruction''s final cycle,')
    $lines.Add('so the first slice credited two stack writes 21 cycles late; widening the')
    $lines.Add('work-RAM range to cover `$21E0`-`$21FF` found it on frame 148 of 3,600.')
    $lines.Add('The third slice''s `PLP` at `46AE` then published the caller''s flags at its')
    $lines.Add('checkpoint, so an interrupt landing on that exact instruction pushed the')
    $lines.Add('pre-pop status — two differing bytes in 5.5 million, both on the stack,')
    $lines.Add('with the census matching either way because the handler restores `P` on the')
    $lines.Add('way out.')
    $lines.Add('')
    $lines.Add('The bank is now 25,029,471 of 45,586,602 cycles native, 55%, and that is')
    $lines.Add('where it stops while the emulator is still here. The remaining 20,557,131')
    $lines.Add('are the per-frame tick at `45:4015`, which calls everything above, plus')
    $lines.Add('`45:4936` and the track loader `45:4BCC`.')
    $lines.Add('')
    $lines.Add('`45:4015` is not merely unwritten: the snapshot rule forbids it. Its')
    $lines.Add('voice loop reaches `45:4148`, `LDA $F5,X`, three times per call, so any')
    $lines.Add('transcript for the tick reads driver state and has to finish before the')
    $lines.Add('next serviceable timer interrupt. The tick''s body is 12,667 master')
    $lines.Add('cycles and the timer period on this route is 5,068, so it never can. The')
    $lines.Add('same rule that admits `4142` at half its calls — 2,054-cycle body, same')
    $lines.Add('5,068-cycle period — rejects the tick at all of them. Porting it would')
    $lines.Add('have produced a transcript builder that declines 3,599 times out of')
    $lines.Add('3,599, which is why the effort went to the palette instead.')
    $lines.Add('')
    $lines.Add('This is the second thing on the list that is blocked by the hybrid rather')
    $lines.Add('than unported, and it has the same resolution as the first: with native')
    $lines.Add('time and interrupts there is no snapshot, so there is nothing to go')
    $lines.Add('stale.')
    $lines.Add('')
    $lines.Add('## The palette shadow buffer')
    $lines.Add('')
    $lines.Add('Gameplay code never writes the VCE. It keeps thirty-two 32-byte blocks of')
    $lines.Add('packed nine-bit GRB in a shadow buffer at `$27B9`-`$2BB8`, marks the blocks')
    $lines.Add('it changed in `$2CCC`-`$2CEB`, and `00:EB30` pushes the dirty ones out to')
    $lines.Add('the VCE ports once a frame. Four routines are now native, and they are')
    $lines.Add('the port''s first move into the video paths:')
    $lines.Add('')
    $lines.Add('| Address | What it is | Native calls | Master cycles |')
    $lines.Add('| --- | --- | ---: | ---: |')
    $lines.Add('| `00:EB30` | The upload: dirty blocks out to the VCE | 3,152 of 3,181 | 6,870,516 |')
    $lines.Add('| `00:E9B6` | Block interpolator: one crossfade step over sixteen colours | 672 | 6,548,946 |')
    $lines.Add('| `00:EBA5` | Block stager: a 32-byte copy plus the dirty flags | 1,121 | 2,629,866 |')
    $lines.Add('| `00:E93C` | The sixteen-bit compare the fade drivers branch on | 8,969 | 758,370 |')
    $lines.Add('')
    $lines.Add('The bottom three are admitted on every call, which is unusual enough to')
    $lines.Add('explain. They are leaves — no `JSR`, no device port, and nothing another')
    $lines.Add('thread of control writes — so there is no horizon to miss and no snapshot')
    $lines.Add('to go stale. An interrupt landing inside one is handled the way it always')
    $lines.Add('is, by a checkpoint.')
    $lines.Add('')
    $lines.Add('The interpolator is the interesting one. Each colour''s three channels are')
    $lines.Add('summed and compared against the step counter at `$2CEC`; colours whose')
    $lines.Add('total falls below it are skipped, which is what staggers the fade, and the')
    $lines.Add('rest have exactly one channel nudged a step toward the target — whichever')
    $lines.Add('of the three differences is largest. The differences are computed with')
    $lines.Add('`SBC` and no clamp, so a channel the shadow has overshot wraps to a large')
    $lines.Add('unsigned value and is treated as furthest away, which is what drives it')
    $lines.Add('back.')
    $lines.Add('')
    $lines.Add('`00:EB30` is the first routine in the project to drive a device other')
    $lines.Add('than the VDC and the PSG. It walks the thirty-two dirty flags, and for')
    $lines.Add('each block it points the colour table at `$0402`/`$0403` and pushes')
    $lines.Add('sixteen colours through `$0404`/`$0405`. Twenty-nine of its 3,599 calls')
    $lines.Add('take a second arm instead, where a single `TIA $27B9, $0404, $0400`')
    $lines.Add('moves all 1,024 bytes at once; that is 1,024 writes against a channel')
    $lines.Add('that carries 512, so the walk declines in front of the first store')
    $lines.Add('rather than truncating the upload. A further 418 calls never reach the')
    $lines.Add('hook because another transcript is suspended when they arrive.')
    $lines.Add('')
    $lines.Add('Admitting it took a two-line widening on each side of the boundary: the')
    $lines.Add('VCE register file was added to the replacement write channel''s')
    $lines.Add('allowlist, and to the core''s wait-state compensation. The second is')
    $lines.Add('what makes the timing come out. `IOWrite` steals a CPU cycle for both')
    $lines.Add('the VDC ports and the VCE, and the interruptible path already advanced')
    $lines.Add('to the cycle *before* a VDC write so that steal landed inside the')
    $lines.Add('declared offset; the VCE was outside that test, so the first run of the')
    $lines.Add('transcript ran three master cycles long per colour — 102 per block,')
    $lines.Add('accumulating all frame. The write values and their order were already')
    $lines.Add('right at that point. Only the clock was wrong, which is exactly the')
    $lines.Add('class of fault a value-only comparison would have passed.')
    $lines.Add('')
    $lines.Add('Three neighbours stay emulated. `00:EAA7`, the fade-to-black stepper,')
    $lines.Add('is 205,012 master cycles per call — roughly nine times the checkpoint')
    $lines.Add('cap, so it cannot be a transcript at any length. `00:E94D` and')
    $lines.Add('`00:EA77`, the two fade drivers, both call the frame-sync spin at')
    $lines.Add('`00:E321`, so a transcript for either would have to span twenty-one')
    $lines.Add('frames.')
    $lines.Add('')
    $lines.Add('Verification is the two-run diff, now over three artifacts: 7,369,685')
    $lines.Add('work-RAM writes covering the shadow buffer, the dirty flags, the')
    $lines.Add('zero-page scratch and the stack; 115,688 VCE register writes; and the')
    $lines.Add('state census. All three are byte-identical to a pure-assembly run of the')
    $lines.Add('same route. Nothing in this slice pushes, so every stack write in that')
    $lines.Add('trace belongs to surrounding code — which is exactly what makes it a')
    $lines.Add('good check on the cycle counts.')
    $lines.Add('')
    $lines.Add('## The bank-switching call wrappers')
    $lines.Add('')
    $lines.Add('Every cross-bank call in the game goes through one of five small wrappers.')
    $lines.Add('The caller leaves a physical bank in `$30` and an entry offset in `$2F`; a')
    $lines.Add('wrapper maps the bank into an MPR slot, jumps through the callee''s `$40xx`')
    $lines.Add('table, and unmaps it on the way out. `$1E`, `$1F`, `$20` and `$21` are the')
    $lines.Add('zero-page shadows of MPR2 through MPR5, and keeping the shadow and the')
    $lines.Add('register in step is what makes the idiom re-entrant: a wrapper that')
    $lines.Add('preempts another restores both from its own stack frame.')
    $lines.Add('')
    $lines.Add('Nine entry points are native, admitted on every one of 52,911 calls:')
    $lines.Add('')
    $lines.Add('| Address | What it is | Native calls | Master cycles |')
    $lines.Add('| --- | --- | ---: | ---: |')
    $lines.Add('| `00:E241` | Banked call, prologue: save MPR2/MPR3 and map `$30` | 11,599 | 1,322,286 |')
    $lines.Add('| `00:E255` | Banked call, epilogue: take the result, restore MPR2/MPR3 | 11,599 | 1,043,910 |')
    $lines.Add('| `00:E262` | The trampoline: build `$40xx` in `$2CBE`/`$2CBF` | 11,671 | 700,260 |')
    $lines.Add('| `00:ECC6` | The VBlank handler''s music call: map banks `$45`-`$47` | 3,503 | 567,486 |')
    $lines.Add('| `00:E219` | Map the fixed bank pair for the mode at `$55` | 3,523 | 422,760 |')
    $lines.Add('| `00:ECE2` | Restore MPR2-MPR4 after it | 3,503 | 409,851 |')
    $lines.Add('| `00:E20D` | Map `$30` into MPR4/MPR5 | 1,861 | 133,992 |')
    $lines.Add('| `00:ECF2` | Point `$47`/`$48` at `$4003` and enter the sequencer | 3,503 | 126,108 |')
    $lines.Add('| `00:E200` | The asset resolver''s tail | 1,861 | 122,826 |')
    $lines.Add('')
    $lines.Add('Four of those are heads and five are the addresses a wrapper''s call comes')
    $lines.Add('back to, because a wrapper exists to call something and the transcript has')
    $lines.Add('to stop in front of that call. Two of the stops are indirect jumps,')
    $lines.Add('`00:E26E` and `00:ECFA`, and the walk stops in front of both rather than')
    $lines.Add('resolving them: each reads a vector the same transcript just wrote, so')
    $lines.Add('handing the jump back to the core costs six instructions of coverage and')
    $lines.Add('removes the only place this module would have to compute a resume bank.')
    $lines.Add('')
    $lines.Add('This is the first slice whose transcripts change the bank mapping. `TAM`')
    $lines.Add('is expressible because `CpuRegisters` carries all eight `MPR` slots and a')
    $lines.Add('checkpoint applies them, so the only real question was whether anything')
    $lines.Add('here reads state an interrupt handler leaves changed. Nothing does. The')
    $lines.Add('VBlank handler''s own wrapper at `00:ECC6` is the sole interrupt-context')
    $lines.Add('writer of `$1E`-`$21` in the whole game, and it restores all three from')
    $lines.Add('the stack before it returns; the timer driver never touches them. That was')
    $lines.Add('settled from a work-RAM write trace of the four addresses before any code')
    $lines.Add('was written — the same two-query screen that ruled out `45:4015`.')
    $lines.Add('')
    $lines.Add('Verification is the two-run diff over 6,389,776 work-RAM writes and the')
    $lines.Add('state census, both byte-identical to a pure-assembly run of the same route.')
    $lines.Add('The whole stack page is in scope rather than a window of it: this family is')
    $lines.Add('nothing but stack-balanced save and restore, and its pushes land wherever')
    $lines.Add('the caller left `S`, spanning `$21E2` through `$21FD` on this route.')
    $lines.Add('')
    $lines.Add('`00:ED5E` is the fifth wrapper and is deliberately absent. It is entered')
    $lines.Add('with two of its four stack slots already pushed by its caller, so its entry')
    $lines.Add('contract belongs to the caller rather than the routine, and it is 23,000')
    $lines.Add('master cycles across 72 calls — 0.4% of this family.')
    $lines.Add('')
    $lines.Add('## The VDC interrupt handler')
    $lines.Add('')
    $lines.Add('`00:E058` is the display interrupt. It reads the VDC status register at')
    $lines.Add('`$0000` into `$01` and dispatches on it: the VBlank bit sends it to')
    $lines.Add('`00:E0EB`, everything else to `00:E136`. The head is not portable — its')
    $lines.Add('whole decision is a byte from a device register, and a replacement')
    $lines.Add('callback receives work RAM and registers only — but nothing downstream of')
    $lines.Add('that read needs the device again, so the bodies are.')
    $lines.Add('')
    $lines.Add('Six entry points are native. Hooked on their own they are admitted on')
    $lines.Add('every one of 13,932 calls; the counts below are from the deployed')
    $lines.Add('registry, where the interrupt sometimes arrives while a main-thread')
    $lines.Add('transcript is suspended and the handler runs emulated for the same')
    $lines.Add('no-nesting reason the sound driver does:')
    $lines.Add('')
    $lines.Add('| Address | What it is | Native calls | Master cycles |')
    $lines.Add('| --- | --- | ---: | ---: |')
    $lines.Add('| `00:E0EB` | VBlank prologue: program VDC `$08`/`$07`/`$05` from the RAM shadows | 2,311 of 2,748 | 457,578 |')
    $lines.Add('| `00:E123` | Epilogue after the music call: restore MPR6 and unwind | 3,132 of 3,212 | 328,860 |')
    $lines.Add('| `00:E136` | Non-VBlank arm: reprogram the split, tick `$222A` | 1,313 of 3,599 | 303,750 |')
    $lines.Add('| `00:E117` | Bump the frame counter at `$00`, take or skip the music call | 3,162 of 3,599 | 182,304 |')
    $lines.Add('| `00:E183` | Secondary handler''s epilogue | 351 of 387 | 43,173 |')
    $lines.Add('| `00:E179` | Secondary handler''s prologue | 351 of 387 | 20,007 |')
    $lines.Add('')
    $lines.Add('Every walk stops one instruction short of its `RTI`, in front of the `PLP`')
    $lines.Add('that restores the interrupted status byte, and that is the whole lesson of')
    $lines.Add('the slice. The core samples the IRQ at `LastCycle`, which for `PLP` runs')
    $lines.Add('*before* the pop; a checkpoint carries one register set and samples with')
    $lines.Add('it, so a transcript covering `PLP` samples with the restored mask instead')
    $lines.Add('of the one the instruction ran under. Assembly services a timer interrupt')
    $lines.Add('between that `PLP` and the `RTI`; the first draft of this slice swallowed')
    $lines.Add('it, and the state census caught it. This is the same fault the sound')
    $lines.Add('driver''s `CLI` produced from the other direction, and the general rule is')
    $lines.Add('now stated once: an instruction that changes `I` cannot be summarised by a')
    $lines.Add('checkpoint that publishes its result.')
    $lines.Add('')
    $lines.Add('That mattered because the handler does not run masked throughout, which')
    $lines.Add('is not obvious: an interrupt sequence sets `I`, and nothing in the handler')
    $lines.Add('clears it. Bank `45` does. Three of its critical sections end with an')
    $lines.Add('unconditional `CLI` rather than restoring the caller''s mask — `45:4872`,')
    $lines.Add('`45:49F2` and `45:47EE`, 376 executions between them — so the music tick')
    $lines.Add('below `00:ECC6` hands the handler back with interrupts enabled. Counting')
    $lines.Add('interrupt entries in an assembly write trace of the stack page puts 78 of')
    $lines.Add('them inside these six walks, every one at or after `00:E123` and none')
    $lines.Add('before a `JSR $ECC6`.')
    $lines.Add('')
    $lines.Add('The shadowing bill is small in spite of that: 64 driver entries and')
    $lines.Add('36,330 cycles, 2.7% of the slice''s own 1,350,720, because the unmasked')
    $lines.Add('stretch is the short one.')
    $lines.Add('')
    $lines.Add('Verification is the two-run diff against pure assembly over three')
    $lines.Add('artifacts: 4,989,037 work-RAM writes including the whole stack page,')
    $lines.Add('2,036,234 writes to the VDC''s three base ports censused directly with')
    $lines.Add('`--bus-write-range`, and the state census. The stack page is what makes')
    $lines.Add('the interrupt fault visible at all, because a swallowed interrupt moves')
    $lines.Add('where its handler pushes.')
    $lines.Add('')
    $lines.Add('## The graphics upload paths')
    $lines.Add('')
    $lines.Add('`00:E3C0`–`E47C` is one idiom three times over: point a VDC or VCE write')
    $lines.Add('pointer at a base address, then push words through the data ports until a')
    $lines.Add('counter runs out. Nothing in the region reads a device register, so unlike')
    $lines.Add('the interrupt dispatcher above it is portable end to end.')
    $lines.Add('')
    $lines.Add('| Address | What it is | Native calls | Master cycles |')
    $lines.Add('| --- | --- | ---: | ---: |')
    $lines.Add('| `00:E439` | Stream-record loop: the decoder''s continuations | 326 of 326 | 5,883,540 |')
    $lines.Add('| `00:E41F` | The generic word-stream decoder''s head | 41 of 41 | 813,204 |')
    $lines.Add('| `00:E403` | VCE colour-table clear loop | 16 of 16 | 128,136 |')
    $lines.Add('| `00:E3FC` | VCE colour-table clear | 4 of 4 | 37,704 |')
    $lines.Add('| `00:E3D7` | VRAM clear loop | 4 of 4 | 25,524 |')
    $lines.Add('| `00:E3C0` | Clear 512 VRAM words from `$7800` | 2 of 2 | 20,736 |')
    $lines.Add('')
    $lines.Add('The decoder is the whole prize, and it is the first thing in the port that')
    $lines.Add('does not fit in a transcript. One call is 167,000 master cycles, 3,400 port')
    $lines.Add('writes and 12,400 instructions, against a channel of 512 writes and 2,048')
    $lines.Add('checkpoints. So it chunks. Each routine has two entry points — its head, and')
    $lines.Add('the head of the loop — and a transcript ends in front of the backward branch')
    $lines.Add('that closes the loop. The core executes that branch, lands on the loop head,')
    $lines.Add('and calls the replacement again for the next chunk. Nothing in the core')
    $lines.Add('changed to allow this: the hook already fires on any fetch at a registered')
    $lines.Add('address, and the replacement contract already ends by handing one instruction')
    $lines.Add('back. The 41 decoder calls become 41 head transcripts and 326 continuations,')
    $lines.Add('and the leak is the branch itself — eighteen master cycles a chunk, 5,868 on')
    $lines.Add('the route.')
    $lines.Add('')
    $lines.Add('Where to chunk is a budget decision, not a fixed count. `00:E479` reads the')
    $lines.Add('next stream record''s command byte before committing to it and stops if that')
    $lines.Add('record would not fit in what is left of either channel, so a transcript never')
    $lines.Add('abandons work it has already transcribed. A record is at most 127 words on')
    $lines.Add('this route — 258 writes — so the first record of a chunk always fits and the')
    $lines.Add('walk always makes progress. That is the property that matters: a chunk that')
    $lines.Add('made no progress would loop for ever.')
    $lines.Add('')
    $lines.Add('The other precondition was shared state. `$2F`/`$30` is the game''s')
    $lines.Add('general-purpose source pointer, written from some 340 sites across eight')
    $lines.Add('banks, and a transcript that reads it is only sound if nothing writes it')
    $lines.Add('underneath. That was measured rather than argued: bracketing each of the 41')
    $lines.Add('calls between its header''s `$06` store at `00:E422` and its last data-port')
    $lines.Add('write, all 23,300 writes to `$2D`-`$30`, `$37` and `$38` inside those windows')
    $lines.Add('come from the decoder itself, and none from interrupt context.')
    $lines.Add('')
    $lines.Add('The shadowing bill runs the other way here. The VDC handler''s transcripts')
    $lines.Add('shadowed the sound driver from inside interrupt context; these are main-thread')
    $lines.Add('transcripts averaging 18,000 cycles, more than three timer periods, so the')
    $lines.Add('interrupts that land inside them cannot reach their own hooks. 6,908,844')
    $lines.Add('native cycles cost 317,196 elsewhere across 956 entries — 130,122 in the sound')
    $lines.Add('driver, 165,123 in the music sequencer, 21,348 in the VDC handler and its')
    $lines.Add('bank wrapper, 603 in the palette upload. Net `+6,591,648`. At 4.6% of gross')
    $lines.Add('that sits between the bank wrappers'' 15.6% and the VDC handler''s 2.7%, and')
    $lines.Add('it is the cheapest bill any main-thread slice has paid.')
    $lines.Add('')
    $lines.Add('One of the 367 transcripts is abandoned partway through a repeat record and')
    $lines.Add('the assembly finishes it: 45 loop iterations and one record tail out of')
    $lines.Add('6.9M cycles. That is the core''s checkpoint guard doing its job — it resumes a')
    $lines.Add('suspended transcript only when the program counter, stack pointer, registers')
    $lines.Add('and all eight MPR slots match what the checkpoint published, and otherwise')
    $lines.Add('drops it and lets the assembly run. The fallback is by design and the census')
    $lines.Add('is unaffected.')
    $lines.Add('')
    $lines.Add('Verification is the two-run diff against pure assembly over three artifacts:')
    $lines.Add('6,036,242 work-RAM writes covering the decoder''s shared zero page and the')
    $lines.Add('whole stack page, 2,151,922 writes to the VDC and VCE register files censused')
    $lines.Add('directly with `--bus-write-range`, and the state census.')
    $lines.Add('')
    $lines.Add('## The tilemap upload, and an instruction a checkpoint cannot express')
    $lines.Add('')
    $lines.Add('`00:F261`–`F4F1` is the other half of the graphics driver''s port work and')
    $lines.Add('the last coherent video row in the inventory. Where `00:E41F` streams a')
    $lines.Add('prepared word list into VRAM, this decides which row of the scrolling')
    $lines.Add('table has just come into view and pushes it. It is two routines:')
    $lines.Add('`00:F3E7`, which programs a VRAM address into the VDC and writes 32 words')
    $lines.Add('— each a tile index from an MPR-mapped source with an attribute nibble')
    $lines.Add('from a work-RAM array folded in — and `00:F261`, which decides whether the')
    $lines.Add('camera has crossed a tile boundary, resolves the source through the')
    $lines.Add('already-native bank helper at `00:E1FD`, calls the writer once or twice,')
    $lines.Add('and folds the movement back into the camera.')
    $lines.Add('')
    $lines.Add('Nothing in the region reads a device register, so the whole 5.7M row was')
    $lines.Add('phase-1 work. Ten entry points, every one admitted on every call:')
    $lines.Add('')
    $lines.Add('| Address | What it is | Native calls | Master cycles |')
    $lines.Add('| --- | --- | ---: | ---: |')
    $lines.Add('| `00:F426` | BBR''s odd-nibble arm, and the rest of its loop iteration | 8,624 of 8,624 | 2,009,931 |')
    $lines.Add('| `00:F42A` | BBR''s even-nibble arm, and the rest of its loop iteration | 8,624 of 8,624 | 1,862,784 |')
    $lines.Add('| `00:F261` | Scroll driver head | 2,649 of 2,649 | 619,371 |')
    $lines.Add('| `00:F3E7` | Row writer head, to the first `BBR` | 539 of 539 | 176,253 |')
    $lines.Add('| `00:F301` | After the first `JSR $F3E7` | 286 of 286 | 140,214 |')
    $lines.Add('| `00:F2AB` | After `JSR $E1FD`, downward | 286 of 286 | 137,280 |')
    $lines.Add('| `00:F35F` | After `JSR $E1FD`, upward | 232 of 232 | 114,144 |')
    $lines.Add('| `00:F3B5` | After the third `JSR $F3E7` | 232 of 232 | 111,216 |')
    $lines.Add('| `00:F3E4` | After the fourth `JSR $F3E7` | 16 of 16 | 7,488 |')
    $lines.Add('| `00:F330` | After the second `JSR $F3E7` | 5 of 5 | 2,250 |')
    $lines.Add('')
    $lines.Add('Six of those entries are ordinary: a transcript stops in front of a call,')
    $lines.Add('so each of the driver''s six `JSR`s needs its return address registered.')
    $lines.Add('Two of them land on `00:E20D` and `00:E200`, native since the wrapper')
    $lines.Add('slice, so stopping there re-enters that module instead of transcribing')
    $lines.Add('the same eleven instructions again.')
    $lines.Add('')
    $lines.Add('`00:F426` and `00:F42A` are the finding. Every other instruction in the')
    $lines.Add('region charges its cycles in one `ADDCYC` and one `LastCycle`, which is')
    $lines.Add('exactly the shape a checkpoint expresses: advance to the sample, sample,')
    $lines.Add('advance to the end. `BBR` is not one instruction in the core, it is two')
    $lines.Add('macros — `LD_ZP` reads `$27` and samples, then `JR` branches and samples')
    $lines.Add('again — so the machine crosses `00:F423` in four clock steps where a')
    $lines.Add('checkpoint crosses it in two. The interrupt outcome at the boundary is the')
    $lines.Add('same either way, but the steps are where the core tests `next_event`, and')
    $lines.Add('a coarser step runs the video `EventHandler` at a later timestamp. That')
    $lines.Add('shifted a VDC event, which moved a timer interrupt one instruction early')
    $lines.Add('four times in 3,600 frames — twice inside the walk, and twice in assembly')
    $lines.Add('*downstream* of it, which is what made it clear the perturbation outlives')
    $lines.Add('the transcript.')
    $lines.Add('')
    $lines.Add('The fix is to hand `00:F423` back to the core and register the two')
    $lines.Add('addresses the branch arrives at, which is why the loop has 17,248 entries')
    $lines.Add('instead of 539. Splitting the difference is not available: an extra')
    $lines.Add('checkpoint inside the `BBR` would make that boundary an extra interrupt')
    $lines.Add('opportunity the machine does not have. It costs 365,664 master cycles, 77%')
    $lines.Add('of everything still emulated in the region.')
    $lines.Add('')
    $lines.Add('Shared state was measured before the walk was written, and by call windows')
    $lines.Add('from a `00:F261` observer rather than inferred from the writes — the')
    $lines.Add('previous slice''s bracketing trick does not work here, because parts of the')
    $lines.Add('region are also reached independently. Across all 2,649 calls, 92,978')
    $lines.Add('writes inside those windows are the routine''s own; the only others are')
    $lines.Add('8,118 interrupt-frame pushes below the live stack pointer and four stores')
    $lines.Add('to `$20`, an address the region writes only inside the `00:E1FD` it stops')
    $lines.Add('in front of and never reads.')
    $lines.Add('')
    $lines.Add('The shadowing bill is the largest any slice has paid, in proportion and')
    $lines.Add('in absolute terms: 5,180,931 native cycles cost 937,827 elsewhere across')
    $lines.Add('1,717 entries, 923,088 of it the sound driver''s. Net `+4,243,104`. At')
    $lines.Add('18.1% of gross it beats the bank wrappers'' 15.6% on a fifth less native')
    $lines.Add('time, which refines the rule the wrapper slice drew rather than confirming')
    $lines.Add('it: total native time sets the scale, but the shadow window is wider than')
    $lines.Add('the transcript. A driver entry that lands inside one runs emulated while')
    $lines.Add('the transcript stays suspended, so its 563 cycles are shadowed too, and')
    $lines.Add('anything arriving inside *those* is shadowed in turn.')
    $lines.Add('')
    $lines.Add('Verification is the two-run diff against pure assembly over three')
    $lines.Add('artifacts: 6,977,084 work-RAM writes covering the driver''s shared zero')
    $lines.Add('page and the whole stack page, 2,036,234 VDC register writes censused with')
    $lines.Add('`--bus-write-range`, and the state census.')
    $lines.Add('')
    $lines.Add('## What is left')
    $lines.Add('')
    $lines.Add('Everything except the idle spin loop is now a porting target, because')
    $lines.Add('the goal is a runtime with no emulator in it at all — video and audio')
    $lines.Add('included. Earlier reports quoted a narrower "portable pool" that')
    $lines.Add('excluded the graphics drivers; that distinction no longer means')
    $lines.Add('anything and the figure has been dropped rather than carried forward')
    $lines.Add('unverified.')
    $lines.Add('')
    $lines.Add('Set the idle spin loop aside and 707,613,360 cycles are the machine')
    $lines.Add('working; 521,505,306 of them, 73.7%, now run natively. The 186,108,054')
    $lines.Add('still emulated are three blocks:')
    $lines.Add('')
    $lines.Add('| Still emulated outside the spin loop | Master cycles |')
    $lines.Add('| --- | ---: |')
    $lines.Add('| Sound-driver entries the no-nesting rule blocks | 103,317,306 |')
    $lines.Add('| Music sequencer''s tick, blocked by the snapshot rule (bank `45`) | 20,621,289 |')
    $lines.Add('| Remaining video-bank and gameplay code | 62,169,459 |')
    $lines.Add('')
    $lines.Add('Only the third is unported code. The first is the half of driver entries')
    $lines.Add('that arrive while another checkpointed transcript is suspended, which the')
    $lines.Add('core runs emulated because it will not nest a replacement. The second is')
    $lines.Add('the sequencer''s per-frame tick, which the snapshot rule rejects at every')
    $lines.Add('call for the reason given above. Both are limits of the hybrid harness,')
    $lines.Add('both are measured here rather than estimated, and both disappear with the')
    $lines.Add('emulator instead of before it — so 123,938,595 of the 186,108,054 is not')
    $lines.Add('work anyone can do now.')
    $lines.Add('')
    $lines.Add('The third is spread thin: no single address in it is worth more than')
    $lines.Add('0.14% of the machine. The largest coherent groups left are the pad')
    $lines.Add('reader `00:E195` (6.9M), the collision tail `01:CA84` (5.8M) and the')
    $lines.Add('gameplay tails at `03:694A` (5.3M) and `02:586F` (4.1M).')
    $lines.Add('')
    $lines.Add('The pad reader is in that list for form''s sake. `00:E195` strobes the')
    $lines.Add('joypad port at `$1000` and then *reads it back*, five times a frame, and a')
    $lines.Add('replacement callback receives work RAM and registers — nothing else. No')
    $lines.Add('amount of widening the write channel reaches it, which is what the plan')
    $lines.Add('for this region assumed would be enough. The same is true of the VDC')
    $lines.Add('interrupt handler''s head at `00:E058`, whose first act is to read the VDC')
    $lines.Add('status register. A routine that reads a device register cannot be a')
    $lines.Add('transcript at all until that device has a native model, so both are phase')
    $lines.Add('2 work rather than phase 1. Everything downstream of that read is now')
    $lines.Add('native; see the section above.')
    $lines.Add('')
    $lines.Add('Note that this percentage tracks only one of the three axes that')
    $lines.Add('removing the emulator needs: which code executes natively. It does not')
    $lines.Add('track device state or time. A native routine today still writes into')
    $lines.Add('Beetle''s PSG and VDC and runs on Beetle''s clock, so the figure will')
    $lines.Add('plateau well short of "the emulator is removable". See')
    $lines.Add('[EMULATOR_ELIMINATION.md](EMULATOR_ELIMINATION.md) for the phases the')
    $lines.Add('percentage does not cover, and for why the replacement no-nesting rule')
    $lines.Add('is deliberately left unfixed.')
    $lines.Add('')
    $lines.Add('Five whole loops are now native instruction for instruction: the main')
    $lines.Add('actor loop (`01:C510`, `01:C51E`, `01:C520`, `01:C523`, `01:C52C`,')
    $lines.Add('`01:C606`, `01:C654`), the collision sweep (`01:CB48`, `01:CB61`,')
    $lines.Add('`01:CB6B`), the camera-visibility sweep (`00:F4F2`, `00:F4FC`, `00:F523`,')
    $lines.Add('`00:F545`, `00:F54E`), the SAT submission (`08:4036`, `08:4063`,')
    $lines.Add('`08:4075`, `08:4088`, `08:4097`, `08:40C3`, `08:40DF`), and the timer')
    $lines.Add('sound driver (`00:FC00`).')
    $lines.Add('')
    $lines.Add('Cycle share is concentrated. The sprite emitter `08:40DF` is the single')
    $lines.Add('largest native address at 7.59% of all master cycles, ahead of the timer')
    $lines.Add('sound driver `00:FC00` at 5.36% and `01:CC1A` (collision vertical scan)')
    $lines.Add('at 5.29%; the top five together are 23.47% of the machine, more than')
    $lines.Add('half of the 40.35% that runs natively. Most installed routines are never')
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
