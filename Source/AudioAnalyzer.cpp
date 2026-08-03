#include "AudioAnalyzer.h"

namespace
{
    constexpr float bassLoHz = 20.0f,   bassHiHz = 250.0f;
    constexpr float midLoHz  = 250.0f,  midHiHz  = 4000.0f;
    constexpr float trebLoHz = 4000.0f, trebHiHz = 16000.0f;

    // Raw FFT magnitude has no fixed ceiling, so squash it into a soft 0..1
    // range instead of a hard clip. The constant is a "typical loud music"
    // calibration, not a hard spec — nudge it if presets read too hot/cold.
    float squash(float magnitude, float scale = 0.12f)
    {
        return 1.0f - std::exp(-magnitude * scale);
    }

    float onePole(float previous, float target, float coeff)
    {
        return previous + coeff * (target - previous);
    }
}

void AudioAnalyzer::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    fifo.fill(0.0f);
    prevMagnitudes.fill(0.0f);
    fifoIndex = 0;
    fluxAverage = 0.0f;
    bass = 0.0f;
    mid = 0.0f;
    treble = 0.0f;
    level = 0.0f;
    onsetPulse = 0.0f;
}

void AudioAnalyzer::pushBlock(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    float blockSumSquares = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sample += buffer.getSample(ch, i);
        sample /= (float) numChannels;

        blockSumSquares += sample * sample;

        fifo[(size_t) fifoIndex++] = sample;
        if (fifoIndex == fftSize)
        {
            fifoIndex = 0;
            runFFTOnFifo();
        }
    }

    const float rms = std::sqrt(blockSumSquares / (float) numSamples);
    const float target = juce::jlimit(0.0f, 1.0f, rms * 4.0f);
    const float coeff = target > level.load(std::memory_order_relaxed) ? 0.55f : 0.08f;
    level.store(onePole(level.load(std::memory_order_relaxed), target, coeff), std::memory_order_relaxed);
}

void AudioAnalyzer::runFFTOnFifo()
{
    std::copy(fifo.begin(), fifo.end(), fftData.begin());
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable(fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    constexpr int numBins = fftSize / 2;
    const float binHz = (float) (sampleRate / (double) fftSize);

    float bassSum = 0.0f, midSum = 0.0f, trebSum = 0.0f;
    int bassCount = 0, midCount = 0, trebCount = 0;
    float flux = 0.0f;

    for (int bin = 1; bin < numBins; ++bin)
    {
        const float magnitude = fftData[(size_t) bin];
        const float freq = (float) bin * binHz;

        flux += std::max(0.0f, magnitude - prevMagnitudes[(size_t) bin]);
        prevMagnitudes[(size_t) bin] = magnitude;

        if (freq >= bassLoHz && freq < bassHiHz)      { bassSum += magnitude; ++bassCount; }
        else if (freq >= midLoHz && freq < midHiHz)   { midSum += magnitude; ++midCount; }
        else if (freq >= trebLoHz && freq < trebHiHz) { trebSum += magnitude; ++trebCount; }
    }

    const float bassTarget = squash(bassCount > 0 ? bassSum / (float) bassCount : 0.0f);
    const float midTarget  = squash(midCount  > 0 ? midSum  / (float) midCount  : 0.0f);
    const float trebTarget = squash(trebCount > 0 ? trebSum / (float) trebCount : 0.0f);

    bass.store(onePole(bass.load(std::memory_order_relaxed), bassTarget,
                        bassTarget > bass.load(std::memory_order_relaxed) ? 0.5f : 0.15f),
               std::memory_order_relaxed);
    mid.store(onePole(mid.load(std::memory_order_relaxed), midTarget,
                       midTarget > mid.load(std::memory_order_relaxed) ? 0.5f : 0.15f),
              std::memory_order_relaxed);
    treble.store(onePole(treble.load(std::memory_order_relaxed), trebTarget,
                          trebTarget > treble.load(std::memory_order_relaxed) ? 0.5f : 0.15f),
                 std::memory_order_relaxed);

    flux /= (float) numBins;
    const float threshold = fluxAverage * 1.6f + 0.001f;
    if (flux > threshold)
        onsetPulse.store(juce::jlimit(0.0f, 1.0f, (flux - fluxAverage) / (fluxAverage + 0.001f)),
                          std::memory_order_relaxed);

    fluxAverage = onePole(fluxAverage, flux, 0.1f);
}
