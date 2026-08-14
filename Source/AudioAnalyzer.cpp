#include "AudioAnalyzer.h"

namespace
{
    constexpr float bassLoHz = 20.0f,   bassHiHz = 250.0f;
    constexpr float midLoHz  = 250.0f,  midHiHz  = 4000.0f;
    constexpr float trebLoHz = 4000.0f, trebHiHz = 16000.0f;

    // Raw FFT magnitude has no fixed ceiling, so squash it into a soft 0..1
    // range instead of a hard clip. The constant is a "typical loud music"
    // calibration, not a hard spec — nudge it if presets read too hot/cold.
    float squash(float magnitude, float scale = 0.28f)
    {
        return 1.0f - std::exp(-magnitude * scale);
    }

    float onePole(float previous, float target, float coeff)
    {
        return previous + coeff * (target - previous);
    }

    // Auto-gain: divides raw by a slowly-decaying tracker of its own recent
    // peak (with a floor so silence doesn't blow the division up), so
    // reactivity tracks the *dynamics* of whatever's playing rather than
    // its absolute loudness -- quiet source material still reads as
    // punchy, loud material doesn't just pin at the ceiling.
    float updateAutoGain(float raw, float& peak, float decayPerCall)
    {
        peak = std::max(raw, peak * decayPerCall);
        peak = std::max(peak, 0.05f);
        return juce::jlimit(0.0f, 1.6f, raw / peak);
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
    bassAG = 0.0f;
    midAG = 0.0f;
    trebleAG = 0.0f;
    levelAG = 0.0f;
    bassPeak = midPeak = treblePeak = levelPeak = 0.05f;
    for (auto& s : waveform)
        s.store(0.0f, std::memory_order_relaxed);
    waveformWriteIndex.store(0, std::memory_order_relaxed);

    stereoWidth = 0.0f;
    correlation = 1.0f;
    widthSmoothed = 0.0f;
    correlationSmoothed = 1.0f;
    for (auto& s : scopeLeft)
        s.store(0.0f, std::memory_order_relaxed);
    for (auto& s : scopeRight)
        s.store(0.0f, std::memory_order_relaxed);
    scopeWriteIndex.store(0, std::memory_order_relaxed);

    for (auto& s : spectrum)
        s.store(0.0f, std::memory_order_relaxed);
    spectrumSmoothed.fill(0.0f);
    spectrumPeaks.fill(0.05f);
}

void AudioAnalyzer::pushBlock(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    float blockSumSquares = 0.0f;

    // Pre-downmix L/R for stereo width/correlation and the goniometer scope
    // -- everything else in this function still works from the mono `sample`
    // below, so this is purely additive. A mono source (numChannels == 1)
    // naturally reads as R == L, i.e. zero width / full correlation.
    float sumLL = 0.0f, sumRR = 0.0f, sumLR = 0.0f, sumMidSq = 0.0f, sumSideSq = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = buffer.getSample(0, i);
        const float r = numChannels > 1 ? buffer.getSample(1, i) : l;

        sumLL += l * l;
        sumRR += r * r;
        sumLR += l * r;
        const float midSample = (l + r) * 0.5f;
        const float sideSample = (l - r) * 0.5f;
        sumMidSq += midSample * midSample;
        sumSideSq += sideSample * sideSample;

        const int scopeIndex = scopeWriteIndex.load(std::memory_order_relaxed);
        scopeLeft[(size_t) scopeIndex].store(l, std::memory_order_relaxed);
        scopeRight[(size_t) scopeIndex].store(r, std::memory_order_relaxed);
        scopeWriteIndex.store((scopeIndex + 1) % stereoScopeSize, std::memory_order_relaxed);

        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sample += buffer.getSample(ch, i);
        sample /= (float) numChannels;

        blockSumSquares += sample * sample;

        const int wIndex = waveformWriteIndex.load(std::memory_order_relaxed);
        waveform[(size_t) wIndex].store(sample, std::memory_order_relaxed);
        waveformWriteIndex.store((wIndex + 1) % waveformSize, std::memory_order_relaxed);

        fifo[(size_t) fifoIndex++] = sample;
        if (fifoIndex == fftSize)
        {
            fifoIndex = 0;
            runFFTOnFifo();
        }
    }

    const float rms = std::sqrt(blockSumSquares / (float) numSamples);
    const float target = juce::jlimit(0.0f, 1.0f, rms * 7.0f);
    const float coeff = target > level.load(std::memory_order_relaxed) ? 0.55f : 0.08f;
    const float smoothedLevel = onePole(level.load(std::memory_order_relaxed), target, coeff);
    level.store(smoothedLevel, std::memory_order_relaxed);

    const float dt = (float) numSamples / (float) sampleRate;
    const float levelPeakDecay = std::exp(-dt / 8.0f); // ~8s time constant
    levelAG.store(updateAutoGain(smoothedLevel, levelPeak, levelPeakDecay), std::memory_order_relaxed);

    // Both already 0..1 (or -1..1) ratios of the source's own energy, so
    // no auto-gain needed -- just smooth them so they don't flicker frame
    // to frame on quiet/transient material.
    const float widthTarget = juce::jlimit(0.0f, 1.0f, sumSideSq / (sumMidSq + sumSideSq + 1.0e-9f));
    const float correlationTarget = juce::jlimit(-1.0f, 1.0f,
        sumLR / (std::sqrt(sumLL * sumRR) + 1.0e-9f));
    widthSmoothed = onePole(widthSmoothed, widthTarget, 0.2f);
    correlationSmoothed = onePole(correlationSmoothed, correlationTarget, 0.2f);
    stereoWidth.store(widthSmoothed, std::memory_order_relaxed);
    correlation.store(correlationSmoothed, std::memory_order_relaxed);
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

    // Log-spaced spectrum bar buckets, same magnitude data as bass/mid/
    // treble above just cut finer -- log spacing so low-frequency bars
    // (where music actually has most of its perceptible detail) get a fair
    // share of the numSpectrumBars slots instead of being crushed into 1-2
    // bars the way a linear split would.
    constexpr float spectrumLoHz = 40.0f;
    const float spectrumHiHz = std::min((float) (sampleRate * 0.5), 16000.0f);
    const float logLo = std::log2(spectrumLoHz);
    const float logHi = std::log2(spectrumHiHz);
    std::array<float, (size_t) numSpectrumBars> barSums {};
    std::array<int, (size_t) numSpectrumBars> barCounts {};
    barSums.fill(0.0f);
    barCounts.fill(0);

    for (int bin = 1; bin < numBins; ++bin)
    {
        const float magnitude = fftData[(size_t) bin];
        const float freq = (float) bin * binHz;

        flux += std::max(0.0f, magnitude - prevMagnitudes[(size_t) bin]);
        prevMagnitudes[(size_t) bin] = magnitude;

        if (freq >= bassLoHz && freq < bassHiHz)      { bassSum += magnitude; ++bassCount; }
        else if (freq >= midLoHz && freq < midHiHz)   { midSum += magnitude; ++midCount; }
        else if (freq >= trebLoHz && freq < trebHiHz) { trebSum += magnitude; ++trebCount; }

        if (freq >= spectrumLoHz && freq <= spectrumHiHz)
        {
            const float t = (std::log2(freq) - logLo) / (logHi - logLo);
            const int barIndex = juce::jlimit(0, numSpectrumBars - 1, (int) (t * (float) numSpectrumBars));
            barSums[(size_t) barIndex] += magnitude;
            ++barCounts[(size_t) barIndex];
        }
    }

    for (int bar = 0; bar < numSpectrumBars; ++bar)
    {
        const float barTarget = squash(barCounts[(size_t) bar] > 0
                                            ? barSums[(size_t) bar] / (float) barCounts[(size_t) bar]
                                            : 0.0f);
        const float prevSmoothed = spectrumSmoothed[(size_t) bar];
        const float smoothedBar = onePole(prevSmoothed, barTarget, barTarget > prevSmoothed ? 0.5f : 0.15f);
        spectrumSmoothed[(size_t) bar] = smoothedBar;

        float& peak = spectrumPeaks[(size_t) bar];
        spectrum[(size_t) bar].store(updateAutoGain(smoothedBar, peak, std::exp(-((float) fftSize / (float) sampleRate) / 7.0f)),
                                      std::memory_order_relaxed);
    }

    const float bassTarget = squash(bassCount > 0 ? bassSum / (float) bassCount : 0.0f);
    const float midTarget  = squash(midCount  > 0 ? midSum  / (float) midCount  : 0.0f);
    const float trebTarget = squash(trebCount > 0 ? trebSum / (float) trebCount : 0.0f);

    const float smoothedBass = onePole(bass.load(std::memory_order_relaxed), bassTarget,
                                        bassTarget > bass.load(std::memory_order_relaxed) ? 0.5f : 0.15f);
    const float smoothedMid = onePole(mid.load(std::memory_order_relaxed), midTarget,
                                       midTarget > mid.load(std::memory_order_relaxed) ? 0.5f : 0.15f);
    const float smoothedTreble = onePole(treble.load(std::memory_order_relaxed), trebTarget,
                                          trebTarget > treble.load(std::memory_order_relaxed) ? 0.5f : 0.15f);
    bass.store(smoothedBass, std::memory_order_relaxed);
    mid.store(smoothedMid, std::memory_order_relaxed);
    treble.store(smoothedTreble, std::memory_order_relaxed);

    const float hopSeconds = (float) fftSize / (float) sampleRate;
    const float bandPeakDecay = std::exp(-hopSeconds / 7.0f); // ~7s time constant
    bassAG.store(updateAutoGain(smoothedBass, bassPeak, bandPeakDecay), std::memory_order_relaxed);
    midAG.store(updateAutoGain(smoothedMid, midPeak, bandPeakDecay), std::memory_order_relaxed);
    trebleAG.store(updateAutoGain(smoothedTreble, treblePeak, bandPeakDecay), std::memory_order_relaxed);

    flux /= (float) numBins;
    const float threshold = fluxAverage * 1.35f + 0.0006f;
    if (flux > threshold)
        onsetPulse.store(juce::jlimit(0.0f, 1.0f, (flux - fluxAverage) / (fluxAverage + 0.001f)),
                          std::memory_order_relaxed);

    fluxAverage = onePole(fluxAverage, flux, 0.1f);
}

void AudioAnalyzer::copyWaveform(std::array<float, (size_t) waveformSize>& out) const noexcept
{
    const int start = waveformWriteIndex.load(std::memory_order_relaxed);
    for (int i = 0; i < waveformSize; ++i)
    {
        const int idx = (start + i) % waveformSize;
        out[(size_t) i] = waveform[(size_t) idx].load(std::memory_order_relaxed);
    }
}

void AudioAnalyzer::copyStereoScope(std::array<float, (size_t) stereoScopeSize>& left,
                                     std::array<float, (size_t) stereoScopeSize>& right) const noexcept
{
    const int start = scopeWriteIndex.load(std::memory_order_relaxed);
    for (int i = 0; i < stereoScopeSize; ++i)
    {
        const int idx = (start + i) % stereoScopeSize;
        left[(size_t) i] = scopeLeft[(size_t) idx].load(std::memory_order_relaxed);
        right[(size_t) i] = scopeRight[(size_t) idx].load(std::memory_order_relaxed);
    }
}

void AudioAnalyzer::copySpectrum(std::array<float, (size_t) numSpectrumBars>& out) const noexcept
{
    for (int i = 0; i < numSpectrumBars; ++i)
        out[(size_t) i] = spectrum[(size_t) i].load(std::memory_order_relaxed);
}
