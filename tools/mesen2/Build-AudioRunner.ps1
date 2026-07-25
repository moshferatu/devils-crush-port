# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $MesenSourceDirectory,

    [Parameter(Mandatory = $true)]
    [string] $MesenAppDirectory,

    [string] $OutputDirectory = "out/tools/mesen2-audio-runner"
)

$ErrorActionPreference = "Stop"

$source = (Resolve-Path -LiteralPath $MesenSourceDirectory).Path
$app = (Resolve-Path -LiteralPath $MesenAppDirectory).Path
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
if ([System.IO.Directory]::Exists($output)) {
    if ([System.IO.Directory]::GetFileSystemEntries($output).Length -gt 0) {
        throw "Audio-runner output directory is not empty: $output"
    }
}
[System.IO.Directory]::CreateDirectory($output) | Out-Null

$sourceUi = Join-Path $source "UI"
$sourceNuGet = Join-Path $source "NuGet.Config"
foreach ($required in @(
    $sourceUi,
    $sourceNuGet,
    (Join-Path $app "MesenCore.dll"),
    (Join-Path $app "libHarfBuzzSharp.dll"),
    (Join-Path $app "libSkiaSharp.dll")
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required Mesen 2.1.1 input is missing: $required"
    }
}

Copy-Item -LiteralPath $sourceUi -Destination $output -Recurse
Copy-Item -LiteralPath $sourceNuGet -Destination $output
$ui = Join-Path $output "UI"

foreach ($relativePath in @(
    "Utilities/CommandLineHelper.cs",
    "Utilities/TestRunner.cs",
    "UI.csproj"
)) {
    $normalizePath = Join-Path $ui $relativePath
    $normalizeText = [System.IO.File]::ReadAllText($normalizePath)
    if (-not $normalizeText.Contains("`r`n")) {
        [System.IO.File]::WriteAllText(
            $normalizePath,
            $normalizeText.Replace("`n", "`r`n"))
    }
}

function Replace-ExactText {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $OldText,
        [Parameter(Mandatory = $true)][string] $NewText
    )

    $text = [System.IO.File]::ReadAllText($Path)
    if (-not $text.Contains($OldText)) {
        throw "Pinned Mesen source no longer matches the audio patch: $Path"
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $text.Replace($OldText, $NewText))
}

$commandLinePath = Join-Path $ui "Utilities/CommandLineHelper.cs"
Replace-ExactText `
    -Path $commandLinePath `
    -OldText "public string? MovieToRecord { get; private set; } = null;`r`n" `
    -NewText (
        "public string? MovieToRecord { get; private set; } = null;`r`n" +
        "`tpublic string? WaveToRecord { get; private set; } = null;`r`n"
    )
Replace-ExactText `
    -Path $commandLinePath `
    -OldText (
        "`t`t`t`t`t`t`tMovieToRecord = moviePath;`r`n" +
        "`t`t`t`t`t`t} else if(switchArg.StartsWith(`"timeout=`")) {"
    ) `
    -NewText (
        "`t`t`t`t`t`t`tMovieToRecord = moviePath;`r`n" +
        "`t`t`t`t`t`t} else if(switchArg.StartsWith(`"recordwave=`")) {`r`n" +
        "`t`t`t`t`t`t`tstring[] values = switchArg.Split('=', 2);`r`n" +
        "`t`t`t`t`t`t`tif(values.Length <= 1) {`r`n" +
        "`t`t`t`t`t`t`t`tcontinue;`r`n" +
        "`t`t`t`t`t`t`t}`r`n" +
        "`t`t`t`t`t`t`tstring wavePath = values[1];`r`n" +
        "`t`t`t`t`t`t`tif(!Path.IsPathRooted(wavePath)) {`r`n" +
        "`t`t`t`t`t`t`t`twavePath = Path.Combine(" +
        "Program.OriginalFolder, wavePath);`r`n" +
        "`t`t`t`t`t`t`t}`r`n" +
        "`t`t`t`t`t`t`tWaveToRecord = wavePath;`r`n" +
        "`t`t`t`t`t`t} else if(switchArg.StartsWith(`"timeout=`")) {"
    )

$testRunnerPath = Join-Path $ui "Utilities/TestRunner.cs"
Replace-ExactText `
    -Path $testRunnerPath `
    -OldText (
        "`t`t`tConfigApi.SetEmulationFlag(" +
        "EmulationFlags.MaximumSpeed, true);`r`n" +
        "`t`t`tEmuApi.Resume();"
    ) `
    -NewText (
        "`t`t`tConfigApi.SetEmulationFlag(" +
        "EmulationFlags.MaximumSpeed, true);`r`n" +
        "`t`t`tif(commandLineHelper.WaveToRecord != null) {`r`n" +
        "`t`t`t`tRecordApi.WaveRecord(commandLineHelper.WaveToRecord);`r`n" +
        "`t`t`t}`r`n" +
        "`t`t`tEmuApi.Resume();"
    )
Replace-ExactText `
    -Path $testRunnerPath `
    -OldText "`t`t`tEmuApi.Stop();`r`n`t`t`tEmuApi.Release();" `
    -NewText (
        "`t`t`tif(commandLineHelper.WaveToRecord != null) {`r`n" +
        "`t`t`t`tRecordApi.WaveStop();`r`n" +
        "`t`t`t}`r`n" +
        "`t`t`tEmuApi.Stop();`r`n" +
        "`t`t`tEmuApi.Release();"
    )

$projectPath = Join-Path $ui "UI.csproj"
Replace-ExactText `
    -Path $projectPath `
    -OldText (
        "<Target Name=`"PreBuildWindows`" BeforeTargets=`"PreBuildEvent`" " +
        "Condition=`"'`$(RuntimeIdentifier)'=='win-x64'`">"
    ) `
    -NewText (
        "<Target Name=`"PreBuildWindows`" BeforeTargets=`"PreBuildEvent`" " +
        "Condition=`"'`$(RuntimeIdentifier)'=='win-x64' And " +
        "'`$(SkipMesenDependencies)'!='true'`">"
    )

$dependencyDirectory = Join-Path $output "dependencies"
[System.IO.Directory]::CreateDirectory(
    $dependencyDirectory) | Out-Null
foreach ($name in @(
    "MesenCore.dll",
    "libHarfBuzzSharp.dll",
    "libSkiaSharp.dll"
)) {
    Copy-Item `
        -LiteralPath (Join-Path $app $name) `
        -Destination $dependencyDirectory
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$dependencyArchive = Join-Path $ui "Dependencies.zip"
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $dependencyDirectory,
    $dependencyArchive)

$packages = Join-Path $output "nuget"
& dotnet restore $projectPath `
    -r win-x64 `
    --configfile (Join-Path $output "NuGet.Config") `
    --packages $packages
if ($LASTEXITCODE -ne 0) {
    throw "Mesen audio-runner package restore failed."
}

$runner = Join-Path $output "bin/win-x64/Release"
[System.IO.Directory]::CreateDirectory($runner) | Out-Null
foreach ($name in @(
    "MesenCore.dll",
    "libHarfBuzzSharp.dll",
    "libSkiaSharp.dll"
)) {
    Copy-Item `
        -LiteralPath (Join-Path $app $name) `
        -Destination $runner
}

& dotnet build $projectPath `
    -c Release `
    -r win-x64 `
    --no-restore `
    "-p:RestorePackagesPath=$packages" `
    "-p:SolutionDir=$output" `
    "-p:SkipMesenDependencies=true"
if ($LASTEXITCODE -ne 0) {
    throw "Mesen audio-runner build failed."
}

$coreHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath (
        Join-Path $runner "MesenCore.dll")
).Hash.ToLowerInvariant()
$sourceCoreHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath (
        Join-Path $app "MesenCore.dll")
).Hash.ToLowerInvariant()
if ($coreHash -ne $sourceCoreHash) {
    throw "The audio runner did not preserve the official MesenCore.dll."
}

Write-Output "Mesen 2.1.1 audio runner built."
Write-Output "Runner: $runner"
Write-Output "Unmodified MesenCore.dll SHA-256: $coreHash"
