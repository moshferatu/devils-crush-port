# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $MesenPath,

    [Parameter(Mandatory = $true)]
    [string] $AppPath,

    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [ValidateSet("gameplay", "physics")]
    [string] $Profile = "gameplay",

    [string] $OutputDirectory = "out/traces/mesen2-gameplay"
)

$ErrorActionPreference = "Stop"

$mesenSource = (Resolve-Path -LiteralPath $MesenPath).Path
$app = (Resolve-Path -LiteralPath $AppPath).Path
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$script = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "gameplay_routines.lua"
)).Path
$comparison = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "Compare-GameplayRoutines.ps1"
)).Path

[System.IO.Directory]::CreateDirectory($output) | Out-Null
$mesenHome = Join-Path $output "mesen-home"
[System.IO.Directory]::CreateDirectory($mesenHome) | Out-Null
$mesen = Join-Path $mesenHome "Mesen.exe"
Copy-Item -LiteralPath $mesenSource -Destination $mesen -Force

$rgbPalette = @(
    for ($color = 0; $color -lt 512; $color++) {
        $rawBlue = 36 * ($color -band 0x07)
        $rawRed = 36 * (($color -band 0x38) -shr 3)
        $rawGreen = 36 * (($color -band 0x1C0) -shr 6)
        $red5 = $rawRed -shr 3
        $green6 = $rawGreen -shr 2
        $blue5 = $rawBlue -shr 3
        $red = ($red5 -shl 3) -bor ($red5 -shr 2)
        $green = ($green6 -shl 2) -bor ($green6 -shr 4)
        $blue = ($blue5 -shl 3) -bor ($blue5 -shr 2)
        [uint32] (
            0xFF000000L -bor ($red -shl 16) -bor
            ($green -shl 8) -bor $blue
        )
    }
)

$settings = @{
    Version = "2.1.1"
    PcEngine = @{
        Port1 = @{ Type = "PceTurboTap" }
        Port1A = @{ Type = "PceController" }
        Port1B = @{ Type = "PceController" }
        Port1C = @{ Type = "PceController" }
        Port1D = @{ Type = "PceController" }
        Port1E = @{ Type = "PceController" }
        RamPowerOnState = "Random"
        EnableRandomPowerOnState = $false
        UseHuC6280aAudio = $true
        RemoveSpriteLimit = $false
        DisableFrameSkipping = $true
        Palette = $rgbPalette
    }
    Debug = @{
        ScriptWindow = @{
            AllowIoOsAccess = $true
            AllowNetworkAccess = $false
            ScriptTimeout = 10
        }
    }
}
$settings |
    ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $mesenHome "settings.json")

$beetleRecording = Join-Path $output "beetle-$Profile.csv"
$beetleWorkRam = Join-Path $output "beetle-$Profile-wram.bin"
$beetleTrace = Join-Path $output "beetle-$Profile-routines.csv"
$beetleCalls = Join-Path $output "beetle-$Profile-calls.csv"
$beetleWrites = "$beetleCalls.writes.csv"
$mesenTrace = Join-Path $output "mesen-$Profile-routines.csv"
$mesenWorkRam = Join-Path $output "mesen-$Profile-wram.bin"
$mesenCalls = Join-Path $output "mesen-$Profile-calls.csv"
$mesenWrites = Join-Path $output "mesen-$Profile-writes.csv"
$mesenTraceRepeat = Join-Path $output "mesen-$Profile-routines-repeat.csv"
$mesenWorkRamRepeat = Join-Path $output "mesen-$Profile-wram-repeat.bin"
$mesenCallsRepeat = Join-Path $output "mesen-$Profile-calls-repeat.csv"
$mesenWritesRepeat = Join-Path $output "mesen-$Profile-writes-repeat.csv"

$hooks = if ($Profile -eq "physics") {
    @("00:F1B4", "00:F241", "01:D1A2", "01:DD09")
} else {
    @("00:E195", "01:C3CB", "01:C49E", "03:6966")
}
$appArguments = @(
    "--record-gameplay", $beetleRecording,
    "--canonical-work-ram", $beetleWorkRam
)
foreach ($hook in $hooks) {
    $appArguments += @("--observe-hook", $hook)
}
$appArguments += @(
    "--routine-trace", $beetleTrace,
    "--routine-call-trace", $beetleCalls,
    "--rom", $rom
)
& $app @appArguments
if ($LASTEXITCODE -ne 0) {
    throw "The Beetle gameplay recorder failed with exit code $LASTEXITCODE."
}

function Invoke-MesenGameplayCapture {
    param(
        [Parameter(Mandatory = $true)][string] $TracePath,
        [Parameter(Mandatory = $true)][string] $WorkRamPath,
        [Parameter(Mandatory = $true)][string] $CallPath,
        [Parameter(Mandatory = $true)][string] $WritePath
    )

    $oldTrace = $env:DC_MESEN_GAMEPLAY_TRACE_OUTPUT
    $oldWorkRam = $env:DC_MESEN_GAMEPLAY_WRAM_OUTPUT
    $oldCall = $env:DC_MESEN_GAMEPLAY_CALL_OUTPUT
    $oldWrite = $env:DC_MESEN_GAMEPLAY_WRITE_OUTPUT
    $oldProfile = $env:DC_MESEN_GAMEPLAY_PROFILE
    try {
        $env:DC_MESEN_GAMEPLAY_TRACE_OUTPUT = $TracePath
        $env:DC_MESEN_GAMEPLAY_WRAM_OUTPUT = $WorkRamPath
        $env:DC_MESEN_GAMEPLAY_CALL_OUTPUT = $CallPath
        $env:DC_MESEN_GAMEPLAY_WRITE_OUTPUT = $WritePath
        $env:DC_MESEN_GAMEPLAY_PROFILE = $Profile
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $mesen
        $startInfo.Arguments = (
            '--testRunner --timeout=300 "{0}" "{1}"' -f $script, $rom
        )
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $process = [System.Diagnostics.Process]::Start($startInfo)
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw (
                "Mesen gameplay capture failed with exit code " +
                "$($process.ExitCode)."
            )
        }
    } finally {
        $env:DC_MESEN_GAMEPLAY_TRACE_OUTPUT = $oldTrace
        $env:DC_MESEN_GAMEPLAY_WRAM_OUTPUT = $oldWorkRam
        $env:DC_MESEN_GAMEPLAY_CALL_OUTPUT = $oldCall
        $env:DC_MESEN_GAMEPLAY_WRITE_OUTPUT = $oldWrite
        $env:DC_MESEN_GAMEPLAY_PROFILE = $oldProfile
    }
}

Invoke-MesenGameplayCapture `
    $mesenTrace $mesenWorkRam $mesenCalls $mesenWrites
Invoke-MesenGameplayCapture `
    $mesenTraceRepeat `
    $mesenWorkRamRepeat `
    $mesenCallsRepeat `
    $mesenWritesRepeat

$mesenTraceHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenTrace
).Hash.ToLowerInvariant()
$mesenTraceRepeatHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenTraceRepeat
).Hash.ToLowerInvariant()
if ($mesenTraceHash -ne $mesenTraceRepeatHash) {
    throw "The Mesen gameplay routine trace is not deterministic."
}
$mesenWorkRamHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenWorkRam
).Hash.ToLowerInvariant()
$mesenWorkRamRepeatHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenWorkRamRepeat
).Hash.ToLowerInvariant()
if ($mesenWorkRamHash -ne $mesenWorkRamRepeatHash) {
    throw "The Mesen gameplay work-RAM stream is not deterministic."
}
$mesenCallHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenCalls
).Hash.ToLowerInvariant()
$mesenCallRepeatHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenCallsRepeat
).Hash.ToLowerInvariant()
if ($mesenCallHash -ne $mesenCallRepeatHash) {
    throw "The Mesen gameplay routine-call trace is not deterministic."
}
$mesenWriteHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenWrites
).Hash.ToLowerInvariant()
$mesenWriteRepeatHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenWritesRepeat
).Hash.ToLowerInvariant()
if ($mesenWriteHash -ne $mesenWriteRepeatHash) {
    throw "The Mesen gameplay ordered-write trace is not deterministic."
}

& powershell.exe `
    -NoProfile `
    -ExecutionPolicy Bypass `
    -File $comparison `
    -BeetleTracePath $beetleTrace `
    -MesenTracePath $mesenTrace `
    -BeetleCallPath $beetleCalls `
    -BeetleWritePath $beetleWrites `
    -MesenCallPath $mesenCalls `
    -MesenWritePath $mesenWrites `
    -RomPath $rom `
    -Profile $Profile
if ($LASTEXITCODE -ne 0) {
    throw "The cross-core gameplay routine comparison failed."
}

Write-Output "Mesen $Profile trace SHA-256: $mesenTraceHash"
Write-Output "Mesen $Profile work-RAM SHA-256: $mesenWorkRamHash"
Write-Output "Mesen $Profile call SHA-256: $mesenCallHash"
Write-Output "Mesen $Profile write SHA-256: $mesenWriteHash"
