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

    // Raw 0..1 transient strength computed once per FFT hop. Non-zero only
    // on the hop a transient was detected; the renderer is responsible for
    // decaying this into a visible pulse over subsequent frames.
    float consumeOnsetPulse() noexcept { return onsetPulse.exchange(0.0f, std::memory_order_relaxed); }

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

    float fluxAverage = 0.0f;
};
