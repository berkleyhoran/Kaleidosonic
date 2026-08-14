#pragma once

#include <JuceHeader.h>
#include "AudioAnalyzer.h"
#include "VisualizerParameters.h"

#if JUCE_WINDOWS
#include "SystemAudioLoopback.h"
#endif

class KaleidosonicAudioProcessor : public juce::AudioProcessor
{
public:
    KaleidosonicAudioProcessor();
    ~KaleidosonicAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // The path of the picture loaded via "Load Image..." (Image-reactive
    // presets), if any. Not an APVTS parameter -- a file path isn't
    // automatable/host-meaningful the way a float knob is -- but stored as
    // a plain property directly on the same ValueTree, so it round-trips
    // through getStateInformation/setStateInformation for free and the
    // picture reappears after a saved session reloads.
    juce::String getImagePath() const { return apvts.state.getProperty(imagePathPropertyID, "").toString(); }
    void setImagePath(const juce::String& path) { apvts.state.setProperty(imagePathPropertyID, path, nullptr); }

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };
    AudioAnalyzer analyzer;
    VisualizerParameterRefs paramRefs;

#if JUCE_WINDOWS
    // Standalone-only (checked at *runtime* via wrapperType -- see the
    // comment on setSystemAudioCaptureEnabled's definition for why it
    // can't be a compile-time format check here), Windows-only (compile-
    // time, since this is raw WASAPI/COM): captures whatever the system's
    // current default output device is playing (WASAPI loopback) instead
    // of whatever's selected as the app's normal audio input, so you can
    // visualize "what your speakers are outputting" without routing audio
    // through a virtual cable first. See SystemAudioLoopback.h.
    void setSystemAudioCaptureEnabled(bool enabled);
    bool isSystemAudioCaptureEnabled() const noexcept { return systemAudioCaptureEnabled; }
#endif

private:
    inline static const juce::Identifier imagePathPropertyID { "imagePath" };

    // Writes to a real log file (unlike DBG, which is a no-op in Release
    // builds) so shader compile/link failures are diagnosable even outside
    // a debugger -- see VisualizerRenderer's compileProgram().
    std::unique_ptr<juce::FileLogger> fileLogger;

#if JUCE_WINDOWS
    std::unique_ptr<SystemAudioLoopbackCapture> systemAudioCapture;
    bool systemAudioCaptureEnabled = false;
    // Remembered so turning system-audio capture back off can re-prepare
    // the analyzer at the app's own device rate instead of being stuck at
    // whatever rate the loopback device happened to be running.
    double lastPreparedSampleRate = 44100.0;
    int lastPreparedSamplesPerBlock = 512;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KaleidosonicAudioProcessor)
};
