#pragma once

#include <JuceHeader.h>
#include <array>

// Turns incoming audio into a handful of lock-free floats the render thread
// can sample every frame: bass/mid/treble energy, overall level, and a
// decaying onset ("beat") pulse. All writes happen on the audio thread via
// pushBlock(); all reads happen on the message/render thread via the
// getters, so every shared value is a std::atomic<float>.
class AudioAnalyzer
{
public:
    AudioAnalyzer() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void pushBlock(const juce::AudioBuffer<float>& buffer);

    float getBass()    const noexcept { return bass.load(std::memory_order_relaxed); }
    float getMid()     const noexcept { return mid.load(std::memory_order_relaxed); }
    float getTreble()  const noexcept { return treble.load(std::memory_order_relaxed); }
    float getLevel()   const noexcept { return level.load(std::memory_order_relaxed); }

    // Auto-gained versions: each is the raw value divided by a slowly
    // decaying tracker of its own recent peak, so quiet source material
    // still reads as punchy and loud material doesn't just pin at 1.0 --
    // the visuals react to the *dynamics* of what's playing rather than
    // its absolute loudness. Shaders use these for reactivity by default;
    // the raw getters above stay available for anything that specifically
    // wants absolute level.
    float getBassAutoGain()   const noexcept { return bassAG.load(std::memory_order_relaxed); }
    float getMidAutoGain()    const noexcept { return midAG.load(std::memory_order_relaxed); }
    float getTrebleAutoGain() const noexcept { return trebleAG.load(std::memory_order_relaxed); }
    float getLevelAutoGain()  const noexcept { return levelAG.load(std::memory_order_relaxed); }

    // Raw 0..1 transient strength computed once per FFT hop. Non-zero only
    // on the hop a transient was detected; the renderer is responsible for
    // decaying this into a visible pulse over subsequent frames.
    float consumeOnsetPulse() noexcept { return onsetPulse.exchange(0.0f, std::memory_order_relaxed); }

    static constexpr int waveformSize = 2048;

    // Copies the most recent `waveformSize` mono samples, oldest-first, into
    // `out`. Used by the oscilloscope preset; each sample is its own atomic
    // so this can be called safely from the render thread while the audio
    // thread keeps writing, at the cost of no cross-sample consistency
    // guarantee (fine for a visualizer — a torn read is imperceptible).
    void copyWaveform(std::array<float, (size_t) waveformSize>& out) const noexcept;

    // Stereo "wideness": 0 = pure mono (no side/difference signal), 1 = the
    // side signal is as strong as the mid signal (very wide/decorrelated).
    // Already a ratio of the source's own mid vs. side energy, so unlike
    // the band getters above this needs no separate auto-gain version --
    // it's inherently loudness-independent.
    float getStereoWidth() const noexcept { return stereoWidth.load(std::memory_order_relaxed); }

    // Phase correlation, -1..1: +1 = L and R are identical (mono-compatible),
    // 0 = fully decorrelated (wide stereo), -1 = out of phase (mono-collapse
    // risk). Standard Pearson correlation between the L and R signals.
    float getCorrelation() const noexcept { return correlation.load(std::memory_order_relaxed); }

    static constexpr int stereoScopeSize = 1024;

    // Copies the most recent `stereoScopeSize` L/R sample pairs, oldest
    // first, into `left`/`right` -- the raw data a goniometer/Lissajous
    // preset plots directly (X = L, Y = R per point). Same torn-read
    // tradeoff as copyWaveform above. On a mono source R mirrors L.
    void copyStereoScope(std::array<float, (size_t) stereoScopeSize>& left,
                          std::array<float, (size_t) stereoScopeSize>& right) const noexcept;

    // Real per-band FFT magnitude, log-spaced ~40Hz..16kHz, already
    // squashed/smoothed/auto-gained the same way the collapsed bass/mid/
    // treble scalars above are -- for presets that want an actual spectrum
    // display instead of 3 bands. Same FFT pass as everything else, just
    // bucketed more finely.
    static constexpr int numSpectrumBars = 48;

    void copySpectrum(std::array<float, (size_t) numSpectrumBars>& out) const noexcept;

private:
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder; // 1024

    void runFFTOnFifo();

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> prevMagnitudes {};
    int fifoIndex = 0;

    double sampleRate = 44100.0;

    std::atomic<float> bass { 0.0f };
    std::atomic<float> mid { 0.0f };
    std::atomic<float> treble { 0.0f };
    std::atomic<float> level { 0.0f };
    std::atomic<float> onsetPulse { 0.0f };

    std::atomic<float> bassAG { 0.0f };
    std::atomic<float> midAG { 0.0f };
    std::atomic<float> trebleAG { 0.0f };
    std::atomic<float> levelAG { 0.0f };
    float bassPeak = 0.05f, midPeak = 0.05f, treblePeak = 0.05f, levelPeak = 0.05f;

    float fluxAverage = 0.0f;

    std::array<std::atomic<float>, (size_t) waveformSize> waveform {};
    std::atomic<int> waveformWriteIndex { 0 };

    std::atomic<float> stereoWidth { 0.0f };
    std::atomic<float> correlation { 1.0f };
    float widthSmoothed = 0.0f;
    float correlationSmoothed = 1.0f;

    std::array<std::atomic<float>, (size_t) stereoScopeSize> scopeLeft {};
    std::array<std::atomic<float>, (size_t) stereoScopeSize> scopeRight {};
    std::atomic<int> scopeWriteIndex { 0 };

    std::array<std::atomic<float>, (size_t) numSpectrumBars> spectrum {};
    std::array<float, (size_t) numSpectrumBars> spectrumSmoothed {};
    std::array<float, (size_t) numSpectrumBars> spectrumPeaks {};
};
