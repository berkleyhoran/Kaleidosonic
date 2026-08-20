#include "AudioAnalyzer.h"
#include <limits>

namespace
{
    constexpr float bassLoHz = 20.0f,   bassHiHz = 250.0f;
    constexpr float midLoHz  = 250.0f,  midHiHz  = 4000.0f;
    constexpr float trebLoHz = 4000.0f, trebHiHz = 16000.0f;

    // Pre-analysis EQ band centers -- the shelves sit right at the same
    // 250Hz/4kHz crossovers bass/mid/treble are bucketed at above, so the
    // three EQ sliders visibly move the same energy the meters/spectrum
    // read. Mid is a peaking bell roughly centered between the two shelves
    // (geometric mean of 250 and 4000 is ~1000Hz).
    constexpr float eqLowShelfHz = 250.0f;
    constexpr float eqHighShelfHz = 4000.0f;
    constexpr float eqMidHz = 1000.0f;
    constexpr float eqMidQ = 0.7f;
    constexpr float eqShelfQ = 0.7071f; // Butterworth-ish, standard shelf default

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

    eqLowShelfL.reset();
    eqLowShelfR.reset();
    eqMidPeakL.reset();
    eqMidPeakR.reset();
    eqHighShelfL.reset();
    eqHighShelfR.reset();
    // Re-derive coefficients at the new sample rate even if the dB targets
    // themselves haven't changed -- appliedEq*Db tracks what was last
    // *computed*, and a shelf/peak filter's coefficients are sample-rate
    // dependent, so a rate change alone must force a recompute.
    appliedEqLowDb = appliedEqMidDb = appliedEqHighDb = std::numeric_limits<float>::quiet_NaN();
}

void AudioAnalyzer::setEqGains(float lowDb, float midDb, float highDb) noexcept
{
    eqLowDbTarget.store(lowDb, std::memory_order_relaxed);
    eqMidDbTarget.store(midDb, std::memory_order_relaxed);
    eqHighDbTarget.store(highDb, std::memory_order_relaxed);
}

void AudioAnalyzer::updateEqCoefficients(float lowDb, float midDb, float highDb)
{
    const auto lowGain = juce::Decibels::decibelsToGain(lowDb);
    const auto midGain = juce::Decibels::decibelsToGain(midDb);
    const auto highGain = juce::Decibels::decibelsToGain(highDb);

    auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, eqLowShelfHz, eqShelfQ, lowGain);
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, eqMidHz, eqMidQ, midGain);
    auto highCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, eqHighShelfHz, eqShelfQ, highGain);

    eqLowShelfL.coefficients = lowCoeffs;
    eqLowShelfR.coefficients = lowCoeffs;
    eqMidPeakL.coefficients = midCoeffs;
    eqMidPeakR.coefficients = midCoeffs;
    eqHighShelfL.coefficients = highCoeffs;
    eqHighShelfR.coefficients = highCoeffs;
}

void AudioAnalyzer::pushBlock(const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    float blockSumSquares = 0.0f;

    // Recompute EQ coefficients only when a control actually moved (or the
    // sample rate changed -- see prepare()'s NaN reset), not every block --
    // cheap either way, but there's no reason to redo three filter designs
    // per callback when nothing changed.
    {
        const float wantLowDb = eqLowDbTarget.load(std::memory_order_relaxed);
        const float wantMidDb = eqMidDbTarget.load(std::memory_order_relaxed);
        const float wantHighDb = eqHighDbTarget.load(std::memory_order_relaxed);
        if (wantLowDb != appliedEqLowDb || wantMidDb != appliedEqMidDb || wantHighDb != appliedEqHighDb)
        {
            updateEqCoefficients(wantLowDb, wantMidDb, wantHighDb);
            appliedEqLowDb = wantLowDb;
            appliedEqMidDb = wantMidDb;
            appliedEqHighDb = wantHighDb;
        }
    }

    // Pre-downmix L/R for stereo width/correlation and the goniometer scope
    // -- everything else in this function still works from the mono `sample`
    // below, so this is purely additive. A mono source (numChannels == 1)
    // naturally reads as R == L, i.e. zero width / full correlation.
    float sumLL = 0.0f, sumRR = 0.0f, sumLR = 0.0f, sumMidSq = 0.0f, sumSideSq = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = buffer.getSample(0, i);
        float r = numChannels > 1 ? buffer.getSample(1, i) : l;

        // Real pre-analysis EQ: reshapes only these local l/r copies --
        // `buffer` itself (what actually reaches the DAW) is never
        // touched, per pushBlock()'s const juce::AudioBuffer<float>&
        // parameter. Every analysis step below (width/correlation, scope,
        // mono downmix, FFT) reads the filtered values from here on.
        l = eqHighShelfL.processSample(eqMidPeakL.processSample(eqLowShelfL.processSample(l)));
        r = eqHighShelfR.processSample(eqMidPeakR.processSample(eqLowShelfR.processSample(r)));

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

        // numChannels is always 1 or 2 (see isBusesLayoutSupported), so this
        // is just the already-filtered l/r averaged -- no need to re-read
        // `buffer` (and re-reading it here would bypass the EQ above).
        const float sample = numChannels > 1 ? (l + r) * 0.5f : l;

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

    // Log-spaced spectrum bars, same magnitude data as bass/mid/treble
    // above just cut finer -- log spacing so low-frequency bars (where
    // music actually has most of its perceptible detail) get a fair share
    // of the numSpectrumBars slots instead of being crushed into 1-2 bars
    // the way a linear split would. Each bar's value is *interpolated*
    // directly from the two nearest FFT bins at that bar's own center
    // frequency, NOT averaged from whichever bins happen to fall in its
    // range -- the FFT's bins are linearly spaced (~43Hz apart at this hop
    // size) while the bars are log-spaced, so in the bass region a bar's
    // frequency range can be narrower than the gap between bins, leaving
    // it with zero bins averaged in (a permanently-silent bar) under the
    // old bucket-averaging approach. Interpolating instead guarantees
    // every bar reads a real, smooth value regardless of how coarse the
    // bin grid is relative to the bar width.
    constexpr float spectrumLoHz = 40.0f;
    const float spectrumHiHz = std::min((float) (sampleRate * 0.5), 16000.0f);
    const float logLo = std::log2(spectrumLoHz);
    const float logHi = std::log2(spectrumHiHz);

    for (int bar = 0; bar < numSpectrumBars; ++bar)
    {
        const float t = (float (bar) + 0.5f) / float (numSpectrumBars);
        const float freq = std::pow(2.0f, logLo + t * (logHi - logLo));
        const float binPos = juce::jlimit(1.0f, (float) (numBins - 2), freq / binHz);
        const int binLo = (int) binPos;
        const float frac = binPos - (float) binLo;
        const float magnitude = fftData[(size_t) binLo] * (1.0f - frac) + fftData[(size_t) (binLo + 1)] * frac;

        const float barTarget = squash(magnitude);
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
