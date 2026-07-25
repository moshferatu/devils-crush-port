# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $MesenAudioRunnerDirectory,

    [Parameter(Mandatory = $true)]
    [string] $AppPath,

    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [string] $OutputDirectory = "out/traces/mesen2-audio"
)

$ErrorActionPreference = "Stop"

$runnerSource = (Resolve-Path -LiteralPath (
    Join-Path $MesenAudioRunnerDirectory "Mesen.exe"
)).Path
$runnerDirectory = [System.IO.Path]::GetDirectoryName($runnerSource)
$app = (Resolve-Path -LiteralPath $AppPath).Path
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$script = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "canonical_audio.lua"
)).Path
$audioComparer = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "Compare-CanonicalAudio.ps1"
)).Path
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($output) | Out-Null

$mesenHome = Join-Path $output "mesen-home"
[System.IO.Directory]::CreateDirectory($mesenHome) | Out-Null
Copy-Item `
    -Path (Join-Path $runnerDirectory "*") `
    -Destination $mesenHome `
    -Recurse `
    -Force
$mesen = Join-Path $mesenHome "Mesen.exe"

$settings = @{
    Version = "2.1.1"
    Audio = @{
        SampleRate = "_44100"
        DisableDynamicSampleRate = $true
        MasterVolume = 100
        ReverbEnabled = $false
        CrossFeedEnabled = $false
        EnableEqualizer = $false
    }
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
    }
    Debug = @{
        ScriptWindow = @{
            AllowIoOsAccess = $false
            AllowNetworkAccess = $false
            ScriptTimeout = 10
        }
    }
}
$settings | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $mesenHome "settings.json")

$beetleCsv = Join-Path $output "beetle-canonical.csv"
$beetleWave = Join-Path $output "beetle-audio.wav"
$mesenWave = Join-Path $output "mesen-audio.wav"
$mesenRepeatWave = Join-Path $output "mesen-audio-repeat.wav"
$reportPath = Join-Path $output "comparison.json"

& $app `
    --record-canonical $beetleCsv `
    --canonical-audio $beetleWave `
    --rom $rom
if ($LASTEXITCODE -ne 0) {
    throw "The Beetle canonical audio recorder failed."
}

function Invoke-MesenAudioCapture {
    param([Parameter(Mandatory = $true)][string] $WavePath)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $mesen
    $startInfo.Arguments = (
        '--testRunner --timeout=60 --recordwave="{0}" ' +
        '--audio.sampleRate=_44100 ' +
        '--audio.disableDynamicSampleRate=true "{1}" "{2}"'
    ) -f $WavePath, $script, $rom
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw (
            "Mesen audio runner failed with exit code " +
            "$($process.ExitCode)."
        )
    }
}

Invoke-MesenAudioCapture -WavePath $mesenWave
Invoke-MesenAudioCapture -WavePath $mesenRepeatWave

& $audioComparer `
    -BeetleWavePath $beetleWave `
    -MesenWavePath $mesenWave `
    -MesenRepeatWavePath $mesenRepeatWave `
    -ReportPath $reportPath | Out-Null

$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
Write-Output "Mesen2 canonical audio comparison complete."
Write-Output (
    "Stereo frames: Beetle {0}, Mesen {1}, difference {2}" -f
    $report.beetleStereoFrames,
    $report.mesenStereoFrames,
    $report.stereoFrameDifference
)
Write-Output (
    "Best alignment: {0} samples ({1:N6} ms)" -f
    $report.bestAlignmentSamples,
    $report.bestAlignmentMilliseconds
)
Write-Output (
    "10 ms RMS-envelope correlation: {0:N9}" -f
    $report.rmsEnvelopeCorrelation
)
Write-Output (
    "Mesen repeat capture byte-exact: {0}" -f
    $report.mesenRepeatExact
)
Write-Output "Report: $reportPath"
