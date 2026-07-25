# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $MesenPath,

    [Parameter(Mandatory = $true)]
    [string] $AppPath,

    [Parameter(Mandatory = $true)]
    [string] $RomPath,

    [string] $OutputDirectory = "out/traces/mesen2"
)

$ErrorActionPreference = "Stop"

$mesenSource = (Resolve-Path -LiteralPath $MesenPath).Path
$app = (Resolve-Path -LiteralPath $AppPath).Path
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
$script = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot "canonical_wram.lua"
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
        [uint32](
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
$settingsPath = Join-Path $mesenHome "settings.json"
$settings | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $settingsPath

$beetleCsv = Join-Path $output "beetle-canonical.csv"
$beetleRam = Join-Path $output "beetle-wram.bin"
$mesenRam = Join-Path $output "mesen-wram.bin"
$mesenRamRepeat = Join-Path $output "mesen-wram-repeat.bin"
$beetleFramebuffer = Join-Path $output "beetle-framebuffer.rgb"
$mesenFramebuffer = Join-Path $output "mesen-framebuffer.rgb"
$mesenFramebufferRepeat = Join-Path $output "mesen-framebuffer-repeat.rgb"
$mesenTrace = Join-Path $output "mesen-write-trace.csv"
$mesenTraceRepeat = Join-Path $output "mesen-write-trace-repeat.csv"
$mesenCpu = Join-Path $output "mesen-cpu.csv"
$mesenCpuRepeat = Join-Path $output "mesen-cpu-repeat.csv"

& $app `
    --record-canonical $beetleCsv `
    --canonical-work-ram $beetleRam `
    --canonical-framebuffer $beetleFramebuffer `
    --rom $rom
if ($LASTEXITCODE -ne 0) {
    throw "The Beetle canonical recorder failed with exit code $LASTEXITCODE."
}

function Invoke-MesenCapture {
    param(
        [Parameter(Mandatory = $true)][string] $WorkRamPath,
        [Parameter(Mandatory = $true)][string] $FramebufferPath,
        [Parameter(Mandatory = $true)][string] $TracePath,
        [Parameter(Mandatory = $true)][string] $CpuPath
    )

    $oldOutput = $env:DC_MESEN_WRAM_OUTPUT
    $oldFramebufferOutput = $env:DC_MESEN_FRAMEBUFFER_OUTPUT
    $oldTraceOutput = $env:DC_MESEN_TRACE_OUTPUT
    $oldCpuOutput = $env:DC_MESEN_CPU_OUTPUT
    try {
        $env:DC_MESEN_WRAM_OUTPUT = $WorkRamPath
        $env:DC_MESEN_FRAMEBUFFER_OUTPUT = $FramebufferPath
        $env:DC_MESEN_TRACE_OUTPUT = $TracePath
        $env:DC_MESEN_CPU_OUTPUT = $CpuPath
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $mesen
        $startInfo.Arguments = (
            '--testRunner --timeout=60 "{0}" "{1}"' -f $script, $rom
        )
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $process = [System.Diagnostics.Process]::Start($startInfo)
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            throw (
                "Mesen test runner failed with exit code " +
                "$($process.ExitCode)."
            )
        }
    } finally {
        $env:DC_MESEN_WRAM_OUTPUT = $oldOutput
        $env:DC_MESEN_FRAMEBUFFER_OUTPUT = $oldFramebufferOutput
        $env:DC_MESEN_TRACE_OUTPUT = $oldTraceOutput
        $env:DC_MESEN_CPU_OUTPUT = $oldCpuOutput
    }
}

Invoke-MesenCapture `
    -WorkRamPath $mesenRam `
    -FramebufferPath $mesenFramebuffer `
    -TracePath $mesenTrace `
    -CpuPath $mesenCpu
Invoke-MesenCapture `
    -WorkRamPath $mesenRamRepeat `
    -FramebufferPath $mesenFramebufferRepeat `
    -TracePath $mesenTraceRepeat `
    -CpuPath $mesenCpuRepeat

$beetleBytes = [System.IO.File]::ReadAllBytes($beetleRam)
$mesenBytes = [System.IO.File]::ReadAllBytes($mesenRam)
$expectedSize = 600 * 8192
if ($beetleBytes.Length -ne $expectedSize) {
    throw "Beetle work-RAM stream has an unexpected size."
}
if ($mesenBytes.Length -ne $expectedSize) {
    throw "Mesen work-RAM stream has an unexpected size."
}
$mesenRamSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenRam
).Hash.ToLowerInvariant()
$mesenRamRepeatSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenRamRepeat
).Hash.ToLowerInvariant()
if ($mesenRamSha256 -ne $mesenRamRepeatSha256) {
    throw "Mesen work RAM is not deterministic across the two captures."
}
$mesenFramebufferRawSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenFramebuffer
).Hash.ToLowerInvariant()
$mesenFramebufferRepeatRawSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenFramebufferRepeat
).Hash.ToLowerInvariant()
$mesenTraceSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenTrace
).Hash.ToLowerInvariant()
$mesenTraceRepeatSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenTraceRepeat
).Hash.ToLowerInvariant()
$mesenCpuSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenCpu
).Hash.ToLowerInvariant()
$mesenCpuRepeatSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $mesenCpuRepeat
).Hash.ToLowerInvariant()

function Get-NormalizedMesenTraceSha256 {
    param([Parameter(Mandatory = $true)][string] $Path)

    $normalized = [System.Text.StringBuilder]::new()
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line.StartsWith("#") -or $line.StartsWith("frame,")) {
            continue
        }

        $fields = $line.Split(",")
        if ($fields.Length -lt 8) {
            throw "Mesen write trace contains an invalid row: $Path"
        }
        [void]$normalized.Append($fields[0])
        [void]$normalized.Append(",")
        [void]$normalized.Append($fields[3])
        [void]$normalized.Append(",")
        [void]$normalized.Append($fields[4])
        [void]$normalized.Append(",")
        [void]$normalized.Append($fields[5])
        [void]$normalized.Append(",")
        [void]$normalized.Append($fields[6])
        [void]$normalized.Append(",")
        [void]$normalized.Append($fields[7])
        [void]$normalized.Append("`n")
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes(
            $normalized.ToString()
        )
        [System.BitConverter]::ToString(
            $sha256.ComputeHash($bytes)
        ).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha256.Dispose()
    }
}

$mesenNormalizedTraceSha256 = Get-NormalizedMesenTraceSha256 `
    -Path $mesenTrace
$mesenNormalizedTraceRepeatSha256 = Get-NormalizedMesenTraceSha256 `
    -Path $mesenTraceRepeat

$mesenCpuRows = @(Import-Csv -LiteralPath $mesenCpu)
if ($mesenCpuRows.Count -ne 600) {
    throw "Mesen CPU boundary stream must contain exactly 600 frames."
}
$mesenStackPointers = [int[]]::new(600)
for ($frame = 0; $frame -lt 600; $frame++) {
    if ([int]$mesenCpuRows[$frame].frame -ne $frame + 1) {
        throw "Mesen CPU boundary stream has an invalid frame sequence."
    }
    $mesenStackPointers[$frame] = [Convert]::ToInt32(
        $mesenCpuRows[$frame].sp.Substring(2),
        16
    )
}

$offsetCounts = [System.Collections.Generic.Dictionary[int, int]]::new()
$frameDifferences = [System.Collections.Generic.List[object]]::new()
$totalDifferences = 0
$maximumDifferences = 0
$exactFrames = 0
$zeroPageDifferences = 0
$zeroPageLastDifferentFrame = 0
$activeStackDifferences = 0
$inactiveStackDifferences = 0
$otherDifferences = 0

for ($frame = 0; $frame -lt 600; $frame++) {
    $different = 0
    $base = $frame * 8192
    for ($offset = 0; $offset -lt 8192; $offset++) {
        if ($beetleBytes[$base + $offset] -eq
            $mesenBytes[$base + $offset]) {
            continue
        }

        $different++
        if ($offset -lt 0x100) {
            $zeroPageDifferences++
            $zeroPageLastDifferentFrame = $frame + 1
        } elseif ($offset -lt 0x200) {
            $stackIndex = $offset - 0x100
            if ($stackIndex -gt $mesenStackPointers[$frame]) {
                $activeStackDifferences++
            } else {
                $inactiveStackDifferences++
            }
        } else {
            $otherDifferences++
        }
        if ($offsetCounts.ContainsKey($offset)) {
            $offsetCounts[$offset]++
        } else {
            $offsetCounts[$offset] = 1
        }
    }

    if ($different -eq 0) {
        $exactFrames++
    }
    $totalDifferences += $different
    $maximumDifferences = [Math]::Max($maximumDifferences, $different)
    $frameDifferences.Add([ordered]@{
        frame = $frame + 1
        differentBytes = $different
    })
}

$offsets = $offsetCounts.GetEnumerator() |
    Sort-Object -Property Value -Descending |
    ForEach-Object {
        $offset = $_.Key
        $firstDifferentFrame = 0
        $lastDifferentFrame = 0
        $beetleValues = [System.Collections.Generic.HashSet[byte]]::new()
        $mesenValues = [System.Collections.Generic.HashSet[byte]]::new()
        for ($frame = 0; $frame -lt 600; $frame++) {
            $index = $frame * 8192 + $offset
            if ($beetleBytes[$index] -eq $mesenBytes[$index]) {
                continue
            }

            if ($firstDifferentFrame -eq 0) {
                $firstDifferentFrame = $frame + 1
            }
            $lastDifferentFrame = $frame + 1
            [void]$beetleValues.Add($beetleBytes[$index])
            [void]$mesenValues.Add($mesenBytes[$index])
        }

        [ordered]@{
            offset = "0x{0:X4}" -f $offset
            frames = $_.Value
            firstDifferentFrame = $firstDifferentFrame
            lastDifferentFrame = $lastDifferentFrame
            beetleValues = @(
                $beetleValues |
                    Sort-Object |
                    ForEach-Object { "0x{0:X2}" -f $_ }
            )
            mesenValues = @(
                $mesenValues |
                    Sort-Object |
                    ForEach-Object { "0x{0:X2}" -f $_ }
            )
        }
    }

function Read-FramebufferStream {
    param([Parameter(Mandatory = $true)][string] $Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 16 -or
        $bytes[0] -ne [byte][char]"D" -or
        $bytes[1] -ne [byte][char]"C" -or
        $bytes[2] -ne [byte][char]"F" -or
        $bytes[3] -ne [byte][char]"1") {
        throw "Framebuffer stream has an invalid header: $Path"
    }

    $width = [System.BitConverter]::ToUInt32($bytes, 4)
    $height = [System.BitConverter]::ToUInt32($bytes, 8)
    $frameCount = [System.BitConverter]::ToUInt32($bytes, 12)
    $frameBytes = [int64]$width * [int64]$height * 3
    $expectedLength = 16 + $frameBytes * $frameCount
    if ($width -eq 0 -or $height -eq 0 -or $frameCount -eq 0 -or
        $expectedLength -ne $bytes.LongLength) {
        throw "Framebuffer stream has invalid geometry or length: $Path"
    }

    [pscustomobject]@{
        Bytes = $bytes
        Width = [int]$width
        Height = [int]$height
        FrameCount = [int]$frameCount
        FrameBytes = [int]$frameBytes
        DataOffset = 16
    }
}

function ConvertTo-LowerHex {
    param([Parameter(Mandatory = $true)][byte[]] $Bytes)

    [System.BitConverter]::ToString($Bytes).Replace("-", "").ToLowerInvariant()
}

Add-Type -TypeDefinition @'
namespace DevilsCrush.Tools
{
    public sealed class FramebufferDifference
    {
        public long TotalDifferentPixels { get; set; }
        public int MaximumDifferentPixelsInFrame { get; set; }
        public int ExactFrames { get; set; }
        public int[] FrameDifferences { get; set; }
    }

    public sealed class FramebufferAlignment
    {
        public int CropX { get; set; }
        public int CropY { get; set; }
        public int FrameShift { get; set; }
        public int DifferentPixels { get; set; }
        public int SampledPixels { get; set; }
    }

    public static class FramebufferComparer
    {
        public static FramebufferAlignment FindAlignment(
            byte[] left,
            int leftWidth,
            int leftHeight,
            int leftFrameBytes,
            byte[] right,
            int rightWidth,
            int rightHeight,
            int rightFrameBytes,
            int frameCount,
            int[] sampleFrames)
        {
            FramebufferAlignment best = null;
            int maximumCropX = rightWidth - leftWidth;
            int maximumCropY = rightHeight - leftHeight + 2;
            for (int frameShift = -2; frameShift <= 2; frameShift++)
            {
                for (int cropY = 0; cropY <= maximumCropY; cropY++)
                {
                    for (int cropX = 0;
                         cropX <= maximumCropX;
                         cropX++)
                    {
                        int differentPixels = 0;
                        int sampledPixels = 0;
                        foreach (int leftFrame in sampleFrames)
                        {
                            int rightFrame = leftFrame + frameShift;
                            if (rightFrame < 0 ||
                                rightFrame >= frameCount)
                            {
                                continue;
                            }

                            for (int y = 0; y < leftHeight; y += 8)
                            {
                                for (int x = 0; x < leftWidth; x += 8)
                                {
                                    int leftIndex =
                                        16 +
                                        leftFrame * leftFrameBytes +
                                        (y * leftWidth + x) * 3;
                                    int rightY = y + cropY;
                                    byte red = 0;
                                    byte green = 0;
                                    byte blue = 0;
                                    if (rightY < rightHeight)
                                    {
                                        int rightIndex =
                                            16 +
                                            rightFrame * rightFrameBytes +
                                            (rightY * rightWidth +
                                                x + cropX) * 3;
                                        red = right[rightIndex];
                                        green = right[rightIndex + 1];
                                        blue = right[rightIndex + 2];
                                    }

                                    sampledPixels++;
                                    if (left[leftIndex] != red ||
                                        left[leftIndex + 1] != green ||
                                        left[leftIndex + 2] != blue)
                                    {
                                        differentPixels++;
                                    }
                                }
                            }
                        }

                        if (best == null ||
                            differentPixels < best.DifferentPixels)
                        {
                            best = new FramebufferAlignment
                            {
                                CropX = cropX,
                                CropY = cropY,
                                FrameShift = frameShift,
                                DifferentPixels = differentPixels,
                                SampledPixels = sampledPixels,
                            };
                        }
                    }
                }
            }

            return best;
        }

        public static FramebufferDifference Compare(
            byte[] left,
            byte[] right,
            int frameBytes)
        {
            if (left == null || right == null ||
                left.Length != right.Length ||
                frameBytes <= 0 ||
                frameBytes % 3 != 0 ||
                left.Length % frameBytes != 0)
            {
                throw new System.ArgumentException(
                    "Framebuffer streams are incompatible.");
            }

            int frameCount = left.Length / frameBytes;
            int[] frameDifferences = new int[frameCount];
            long totalDifferences = 0;
            int maximumDifferences = 0;
            int exactFrames = 0;
            for (int frame = 0; frame < frameCount; frame++)
            {
                int different = 0;
                int end = (frame + 1) * frameBytes;
                for (int offset = frame * frameBytes;
                     offset < end;
                     offset += 3)
                {
                    if (left[offset] != right[offset] ||
                        left[offset + 1] != right[offset + 1] ||
                        left[offset + 2] != right[offset + 2])
                    {
                        different++;
                    }
                }

                frameDifferences[frame] = different;
                totalDifferences += different;
                maximumDifferences = System.Math.Max(
                    maximumDifferences,
                    different);
                if (different == 0)
                {
                    exactFrames++;
                }
            }

            return new FramebufferDifference
            {
                TotalDifferentPixels = totalDifferences,
                MaximumDifferentPixelsInFrame = maximumDifferences,
                ExactFrames = exactFrames,
                FrameDifferences = frameDifferences,
            };
        }
    }
}
'@

$beetleVideo = Read-FramebufferStream -Path $beetleFramebuffer
$mesenVideo = Read-FramebufferStream -Path $mesenFramebuffer
$mesenVideoRepeat = Read-FramebufferStream -Path $mesenFramebufferRepeat
if ($beetleVideo.FrameCount -ne 600 -or
    $mesenVideo.FrameCount -ne 600 -or
    $mesenVideoRepeat.FrameCount -ne 600) {
    throw "Framebuffer streams must contain exactly 600 frames."
}
if ($mesenVideo.Width -lt $beetleVideo.Width -or
    $mesenVideo.Height -lt $beetleVideo.Height -or
    $mesenVideoRepeat.Width -ne $mesenVideo.Width -or
    $mesenVideoRepeat.Height -ne $mesenVideo.Height) {
    throw "Mesen's framebuffer is smaller than Beetle's framebuffer."
}

$sampleFrames = [int[]]@(164, 188, 221, 284, 299, 419, 479, 539)
$bestAlignment = [DevilsCrush.Tools.FramebufferComparer]::FindAlignment(
    $beetleVideo.Bytes,
    $beetleVideo.Width,
    $beetleVideo.Height,
    $beetleVideo.FrameBytes,
    $mesenVideo.Bytes,
    $mesenVideo.Width,
    $mesenVideo.Height,
    $mesenVideo.FrameBytes,
    600,
    $sampleFrames
)
$repeatAlignment = [DevilsCrush.Tools.FramebufferComparer]::FindAlignment(
    $beetleVideo.Bytes,
    $beetleVideo.Width,
    $beetleVideo.Height,
    $beetleVideo.FrameBytes,
    $mesenVideoRepeat.Bytes,
    $mesenVideoRepeat.Width,
    $mesenVideoRepeat.Height,
    $mesenVideoRepeat.FrameBytes,
    600,
    $sampleFrames
)

if ($bestAlignment.FrameShift -ge 0) {
    $beetleFirstFrame = 0
    $mesenFirstFrame = $bestAlignment.FrameShift
    $videoFrameCount = 600 - $bestAlignment.FrameShift
} else {
    $beetleFirstFrame = -$bestAlignment.FrameShift
    $mesenFirstFrame = 0
    $videoFrameCount = 600 + $bestAlignment.FrameShift
}

$normalizedLength = $videoFrameCount * $beetleVideo.FrameBytes
$normalizedBeetle = [byte[]]::new($normalizedLength)
$normalizedMesen = [byte[]]::new($normalizedLength)
[System.Buffer]::BlockCopy(
    $beetleVideo.Bytes,
    $beetleVideo.DataOffset +
        $beetleFirstFrame * $beetleVideo.FrameBytes,
    $normalizedBeetle,
    0,
    $normalizedLength
)
for ($frame = 0; $frame -lt $videoFrameCount; $frame++) {
    for ($y = 0; $y -lt $beetleVideo.Height; $y++) {
        $sourceY = $y + $bestAlignment.CropY
        if ($sourceY -ge $mesenVideo.Height) {
            continue
        }
        $sourceOffset = $mesenVideo.DataOffset +
            ($mesenFirstFrame + $frame) * $mesenVideo.FrameBytes +
            ($sourceY * $mesenVideo.Width +
                $bestAlignment.CropX) * 3
        $destinationOffset = $frame * $beetleVideo.FrameBytes +
            $y * $beetleVideo.Width * 3
        [System.Buffer]::BlockCopy(
            $mesenVideo.Bytes,
            $sourceOffset,
            $normalizedMesen,
            $destinationOffset,
            $beetleVideo.Width * 3
        )
    }
}

$sha256 = [System.Security.Cryptography.SHA256]::Create()
try {
    $beetleVideoSha256 = ConvertTo-LowerHex -Bytes (
        $sha256.ComputeHash($normalizedBeetle)
    )
    $mesenVideoSha256 = ConvertTo-LowerHex -Bytes (
        $sha256.ComputeHash($normalizedMesen)
    )
} finally {
    $sha256.Dispose()
}
$videoDifferences = [DevilsCrush.Tools.FramebufferComparer]::Compare(
    $normalizedBeetle,
    $normalizedMesen,
    $beetleVideo.FrameBytes
)
$videoFrameDifferences = for (
    $frame = 0;
    $frame -lt $videoFrameCount;
    $frame++
) {
    [ordered]@{
        beetleFrame = $beetleFirstFrame + $frame + 1
        mesenFrame = $mesenFirstFrame + $frame + 1
        differentPixels = $videoDifferences.FrameDifferences[$frame]
    }
}

$report = [ordered]@{
    format = "devils-crush-mesen2-canonical-comparison-v5"
    romSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $rom).Hash.ToLower()
    beetleRevision = "ae99235c2139c176c1a8d0fde2957bf701d3cab0"
    mesenVersion = "2.1.1"
    mesenBackupRamNormalization = [ordered]@{
        size = 2048
        headerByteOffset = 5
        mesenInitialValue = "0xA0"
        beetleBaselineValue = "0x88"
    }
    mesenExeSha256 = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $mesenSource
    ).Hash.ToLower()
    frameCount = 600
    bytesPerFrame = 8192
    exactFrames = $exactFrames
    totalDifferentByteObservations = $totalDifferences
    averageDifferentBytesPerFrame = $totalDifferences / 600
    maximumDifferentBytesInFrame = $maximumDifferences
    distinctDifferingOffsets = $offsetCounts.Count
    workRamDifferenceClassification = [ordered]@{
        zeroPageScratchObservations = $zeroPageDifferences
        zeroPageLastDifferentFrame = $zeroPageLastDifferentFrame
        activeStackObservations = $activeStackDifferences
        inactiveStackObservations = $inactiveStackDifferences
        otherObservations = $otherDifferences
    }
    differingOffsets = @($offsets)
    frames = @($frameDifferences)
    mesenDeterminism = [ordered]@{
        captures = 2
        workRamByteExact = $true
        workRamSha256 = $mesenRamSha256
        writeTraceRawByteExact = (
            $mesenTraceSha256 -eq $mesenTraceRepeatSha256
        )
        writeTraceRawSha256 = $mesenTraceSha256
        repeatWriteTraceRawSha256 = $mesenTraceRepeatSha256
        writeEventSequenceExact = (
            $mesenNormalizedTraceSha256 -eq
                $mesenNormalizedTraceRepeatSha256
        )
        writeEventSequenceSha256 = $mesenNormalizedTraceSha256
        repeatWriteEventSequenceSha256 = (
            $mesenNormalizedTraceRepeatSha256
        )
        cpuBoundaryByteExact = $mesenCpuSha256 -eq $mesenCpuRepeatSha256
        cpuBoundarySha256 = $mesenCpuSha256
        repeatCpuBoundarySha256 = $mesenCpuRepeatSha256
        framebufferByteExact = (
            $mesenFramebufferRawSha256 -eq
                $mesenFramebufferRepeatRawSha256
        )
        framebufferSha256 = $mesenFramebufferRawSha256
        repeatFramebufferSha256 = $mesenFramebufferRepeatRawSha256
        repeatFramebufferAlignment = [ordered]@{
            cropX = $repeatAlignment.CropX
            cropY = $repeatAlignment.CropY
            frameShift = $repeatAlignment.FrameShift
            sampleDifferentPixels = $repeatAlignment.DifferentPixels
            sampledPixels = $repeatAlignment.SampledPixels
        }
    }
    framebuffer = [ordered]@{
        format = "DCF1 RGB888"
        beetleGeometry = (
            "{0}x{1}" -f $beetleVideo.Width, $beetleVideo.Height
        )
        mesenGeometry = (
            "{0}x{1}" -f $mesenVideo.Width, $mesenVideo.Height
        )
        mesenCrop = [ordered]@{
            x = $bestAlignment.CropX
            y = $bestAlignment.CropY
            width = $beetleVideo.Width
            height = $beetleVideo.Height
            blackPaddingBottom = [Math]::Max(
                0,
                $bestAlignment.CropY + $beetleVideo.Height -
                    $mesenVideo.Height
            )
        }
        mesenFrameShift = $bestAlignment.FrameShift
        comparedFrames = $videoFrameCount
        pixelsPerFrame = $beetleVideo.FrameBytes / 3
        exactFrames = $videoDifferences.ExactFrames
        totalDifferentPixelObservations = (
            $videoDifferences.TotalDifferentPixels
        )
        averageDifferentPixelsPerFrame = (
            $videoDifferences.TotalDifferentPixels / $videoFrameCount
        )
        maximumDifferentPixelsInFrame = (
            $videoDifferences.MaximumDifferentPixelsInFrame
        )
        byteExact = $beetleVideoSha256 -eq $mesenVideoSha256
        beetleNormalizedSha256 = $beetleVideoSha256
        mesenNormalizedSha256 = $mesenVideoSha256
        alignmentSampleDifferentPixels = $bestAlignment.DifferentPixels
        alignmentSampledPixels = $bestAlignment.SampledPixels
        frames = @($videoFrameDifferences)
    }
}
$reportPath = Join-Path $output "comparison.json"
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath

Write-Output "Mesen2 comparison complete."
Write-Output "Exact frames: $exactFrames / 600"
Write-Output "Different byte observations: $totalDifferences / $expectedSize"
Write-Output "Maximum different bytes in one frame: $maximumDifferences / 8192"
Write-Output "Distinct differing offsets: $($offsetCounts.Count) / 8192"
Write-Output (
    (
        "Mesen repeat capture: work RAM exact={0}, " +
            "write events exact={1}, framebuffer exact={2}"
    ) -f
        $true,
        ($mesenNormalizedTraceSha256 -eq
            $mesenNormalizedTraceRepeatSha256),
        ($mesenFramebufferRawSha256 -eq
            $mesenFramebufferRepeatRawSha256)
)
Write-Output (
    (
        "Work RAM classification: zero-page scratch {0} " +
            "(last frame {1}), inactive stack {2}, " +
            "active stack {3}, other {4}"
    ) -f
        $zeroPageDifferences,
        $zeroPageLastDifferentFrame,
        $inactiveStackDifferences,
        $activeStackDifferences,
        $otherDifferences
)
Write-Output (
    "Mesen repeat alignment: crop ({0},{1}), frame shift {2}" -f
    $repeatAlignment.CropX,
    $repeatAlignment.CropY,
    $repeatAlignment.FrameShift
)
Write-Output (
    "Framebuffer alignment: crop ({0},{1}), frame shift {2}" -f
    $bestAlignment.CropX,
    $bestAlignment.CropY,
    $bestAlignment.FrameShift
)
Write-Output (
    "Exact normalized framebuffer frames: {0} / {1}" -f
    $videoDifferences.ExactFrames,
    $videoFrameCount
)
Write-Output (
    "Different framebuffer pixels: {0} / {1}" -f
    $videoDifferences.TotalDifferentPixels,
    ($videoFrameCount * $beetleVideo.FrameBytes / 3)
)
Write-Output "Report: $reportPath"
