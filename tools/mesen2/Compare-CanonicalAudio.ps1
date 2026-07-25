# SPDX-License-Identifier: GPL-3.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BeetleWavePath,

    [Parameter(Mandatory = $true)]
    [string] $MesenWavePath,

    [string] $MesenRepeatWavePath,

    [string] $ReportPath
)

$ErrorActionPreference = "Stop"

if (-not ("DevilsCrush.Tools.CanonicalAudioComparer" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;

namespace DevilsCrush.Tools
{
    public sealed class WaveData
    {
        public uint SampleRate;
        public short[] Samples = Array.Empty<short>();
        public int StereoFrames { get { return Samples.Length / 2; } }
    }

    public sealed class AudioComparison
    {
        public int BeetleStereoFrames;
        public int MesenStereoFrames;
        public int StereoFrameDifference;
        public int BestAlignmentSamples;
        public double BestAlignmentMilliseconds;
        public double RmsEnvelopeCorrelation;
        public double WaveformCorrelation;
        public double OptimalMesenGain;
        public double GainAdjustedNormalizedRmse;
    }

    public static class CanonicalAudioComparer
    {
        private static ushort U16(byte[] data, int offset)
        {
            return BitConverter.ToUInt16(data, offset);
        }

        private static uint U32(byte[] data, int offset)
        {
            return BitConverter.ToUInt32(data, offset);
        }

        public static WaveData Read(string path)
        {
            byte[] data = File.ReadAllBytes(path);
            if(data.Length < 44
                || data[0] != (byte)'R' || data[1] != (byte)'I'
                || data[2] != (byte)'F' || data[3] != (byte)'F'
                || data[8] != (byte)'W' || data[9] != (byte)'A'
                || data[10] != (byte)'V' || data[11] != (byte)'E'
                || data[12] != (byte)'f' || data[13] != (byte)'m'
                || data[14] != (byte)'t' || data[15] != (byte)' '
                || data[36] != (byte)'d' || data[37] != (byte)'a'
                || data[38] != (byte)'t' || data[39] != (byte)'a'
                || U32(data, 16) != 16 || U16(data, 20) != 1
                || U16(data, 22) != 2 || U16(data, 34) != 16) {
                throw new InvalidDataException(
                    "Expected a canonical 44-byte PCM16 stereo WAV header: "
                    + path);
            }

            uint dataSize = U32(data, 40);
            if(dataSize != data.Length - 44 || (dataSize % 4) != 0) {
                throw new InvalidDataException(
                    "WAV data size is inconsistent: " + path);
            }

            short[] samples = new short[dataSize / 2];
            Buffer.BlockCopy(data, 44, samples, 0, (int)dataSize);
            return new WaveData {
                SampleRate = U32(data, 24),
                Samples = samples
            };
        }

        private static double[] RmsEnvelope(
            WaveData wave,
            int windowFrames)
        {
            int count = wave.StereoFrames / windowFrames;
            double[] result = new double[count];
            for(int window = 0; window < count; window++) {
                double sum = 0;
                int first = window * windowFrames * 2;
                int last = first + windowFrames * 2;
                for(int index = first; index < last; index++) {
                    double sample = wave.Samples[index] / 32768.0;
                    sum += sample * sample;
                }
                result[window] = Math.Sqrt(sum / (windowFrames * 2));
            }
            return result;
        }

        private static double Correlation(
            double[] left,
            double[] right,
            int lag)
        {
            int leftStart = lag < 0 ? -lag : 0;
            int rightStart = lag > 0 ? lag : 0;
            int count = Math.Min(
                left.Length - leftStart,
                right.Length - rightStart);
            double sumLeft = 0;
            double sumRight = 0;
            double sumLeft2 = 0;
            double sumRight2 = 0;
            double sumProduct = 0;
            for(int index = 0; index < count; index++) {
                double a = left[leftStart + index];
                double b = right[rightStart + index];
                sumLeft += a;
                sumRight += b;
                sumLeft2 += a * a;
                sumRight2 += b * b;
                sumProduct += a * b;
            }
            double covariance =
                sumProduct - sumLeft * sumRight / count;
            double varianceLeft =
                sumLeft2 - sumLeft * sumLeft / count;
            double varianceRight =
                sumRight2 - sumRight * sumRight / count;
            return covariance / Math.Sqrt(varianceLeft * varianceRight);
        }

        private static double WaveCorrelation(
            WaveData beetle,
            WaveData mesen,
            int lag,
            int stride)
        {
            int beetleStart = lag < 0 ? -lag : 0;
            int mesenStart = lag > 0 ? lag : 0;
            int count = Math.Min(
                beetle.StereoFrames - beetleStart,
                mesen.StereoFrames - mesenStart);
            double sumA = 0;
            double sumB = 0;
            double sumA2 = 0;
            double sumB2 = 0;
            double sumProduct = 0;
            int observations = 0;
            for(int frame = 0; frame < count; frame += stride) {
                int a = (beetleStart + frame) * 2;
                int b = (mesenStart + frame) * 2;
                double left =
                    (beetle.Samples[a] + beetle.Samples[a + 1]) * 0.5;
                double right =
                    (mesen.Samples[b] + mesen.Samples[b + 1]) * 0.5;
                sumA += left;
                sumB += right;
                sumA2 += left * left;
                sumB2 += right * right;
                sumProduct += left * right;
                observations++;
            }
            double covariance =
                sumProduct - sumA * sumB / observations;
            double varianceA =
                sumA2 - sumA * sumA / observations;
            double varianceB =
                sumB2 - sumB * sumB / observations;
            return covariance / Math.Sqrt(varianceA * varianceB);
        }

        public static AudioComparison Compare(
            WaveData beetle,
            WaveData mesen)
        {
            if(beetle.SampleRate != mesen.SampleRate) {
                throw new InvalidDataException(
                    "Canonical WAV sample rates do not match.");
            }

            int windowFrames = (int)beetle.SampleRate / 100;
            double[] beetleEnvelope =
                RmsEnvelope(beetle, windowFrames);
            double[] mesenEnvelope =
                RmsEnvelope(mesen, windowFrames);
            int bestEnvelopeLag = 0;
            double bestEnvelopeCorrelation =
                Double.NegativeInfinity;
            for(int lag = -50; lag <= 50; lag++) {
                double correlation =
                    Correlation(beetleEnvelope, mesenEnvelope, lag);
                if(correlation > bestEnvelopeCorrelation) {
                    bestEnvelopeCorrelation = correlation;
                    bestEnvelopeLag = lag;
                }
            }

            int center = bestEnvelopeLag * windowFrames;
            int bestSampleLag = center;
            double bestWaveCorrelation = Double.NegativeInfinity;
            for(int lag = center - windowFrames;
                lag <= center + windowFrames;
                lag++) {
                double correlation =
                    WaveCorrelation(beetle, mesen, lag, 8);
                if(correlation > bestWaveCorrelation) {
                    bestWaveCorrelation = correlation;
                    bestSampleLag = lag;
                }
            }

            int beetleStart = bestSampleLag < 0
                ? -bestSampleLag : 0;
            int mesenStart = bestSampleLag > 0
                ? bestSampleLag : 0;
            int frameCount = Math.Min(
                beetle.StereoFrames - beetleStart,
                mesen.StereoFrames - mesenStart);
            double product = 0;
            double mesenEnergy = 0;
            double beetleEnergy = 0;
            for(int frame = 0; frame < frameCount; frame++) {
                for(int channel = 0; channel < 2; channel++) {
                    double a = beetle.Samples[
                        (beetleStart + frame) * 2 + channel];
                    double b = mesen.Samples[
                        (mesenStart + frame) * 2 + channel];
                    product += a * b;
                    beetleEnergy += a * a;
                    mesenEnergy += b * b;
                }
            }
            double gain = product / mesenEnergy;
            double error = 0;
            for(int frame = 0; frame < frameCount; frame++) {
                for(int channel = 0; channel < 2; channel++) {
                    double a = beetle.Samples[
                        (beetleStart + frame) * 2 + channel];
                    double b = mesen.Samples[
                        (mesenStart + frame) * 2 + channel] * gain;
                    double difference = a - b;
                    error += difference * difference;
                }
            }

            return new AudioComparison {
                BeetleStereoFrames = beetle.StereoFrames,
                MesenStereoFrames = mesen.StereoFrames,
                StereoFrameDifference =
                    mesen.StereoFrames - beetle.StereoFrames,
                BestAlignmentSamples = bestSampleLag,
                BestAlignmentMilliseconds =
                    bestSampleLag * 1000.0 / beetle.SampleRate,
                RmsEnvelopeCorrelation = bestEnvelopeCorrelation,
                WaveformCorrelation = bestWaveCorrelation,
                OptimalMesenGain = gain,
                GainAdjustedNormalizedRmse =
                    Math.Sqrt(error / beetleEnergy)
            };
        }
    }
}
'@
}

$beetle = [DevilsCrush.Tools.CanonicalAudioComparer]::Read(
    (Resolve-Path -LiteralPath $BeetleWavePath).Path)
$mesen = [DevilsCrush.Tools.CanonicalAudioComparer]::Read(
    (Resolve-Path -LiteralPath $MesenWavePath).Path)
$comparison = [DevilsCrush.Tools.CanonicalAudioComparer]::Compare(
    $beetle,
    $mesen)
$mesenSha256 = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $MesenWavePath
).Hash.ToLowerInvariant()
$mesenRepeatSha256 = $null
$mesenRepeatExact = $null
if (-not [string]::IsNullOrWhiteSpace($MesenRepeatWavePath)) {
    $mesenRepeatSha256 = (
        Get-FileHash `
            -Algorithm SHA256 `
            -LiteralPath $MesenRepeatWavePath
    ).Hash.ToLowerInvariant()
    $mesenRepeatExact = $mesenSha256 -eq $mesenRepeatSha256
    if (-not $mesenRepeatExact) {
        throw "Mesen canonical audio is not deterministic across captures."
    }
}

$report = [ordered]@{
    format = "devils-crush-canonical-audio-comparison-v1"
    sampleRate = $beetle.SampleRate
    beetleStereoFrames = $comparison.BeetleStereoFrames
    mesenStereoFrames = $comparison.MesenStereoFrames
    stereoFrameDifference = $comparison.StereoFrameDifference
    durationDifferenceMilliseconds = (
        $comparison.StereoFrameDifference * 1000.0 /
            $beetle.SampleRate
    )
    bestAlignmentSamples = $comparison.BestAlignmentSamples
    bestAlignmentMilliseconds = $comparison.BestAlignmentMilliseconds
    rmsEnvelopeCorrelation = $comparison.RmsEnvelopeCorrelation
    waveformCorrelation = $comparison.WaveformCorrelation
    optimalMesenGain = $comparison.OptimalMesenGain
    gainAdjustedNormalizedRmse = (
        $comparison.GainAdjustedNormalizedRmse
    )
    beetleSha256 = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $BeetleWavePath
    ).Hash.ToLowerInvariant()
    mesenSha256 = (
        $mesenSha256
    )
    mesenRepeatSha256 = $mesenRepeatSha256
    mesenRepeatExact = $mesenRepeatExact
}

if (-not [string]::IsNullOrWhiteSpace($ReportPath)) {
    $fullReportPath = [System.IO.Path]::GetFullPath($ReportPath)
    $reportDirectory = [System.IO.Path]::GetDirectoryName($fullReportPath)
    if (-not [string]::IsNullOrWhiteSpace($reportDirectory)) {
        [System.IO.Directory]::CreateDirectory(
            $reportDirectory) | Out-Null
    }
    $report | ConvertTo-Json -Depth 3 |
        Set-Content -LiteralPath $fullReportPath
}

$report
