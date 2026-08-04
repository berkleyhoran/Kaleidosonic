#pragma once

#include <JuceHeader.h>
#include "AudioAnalyzer.h"
#include "VisualizerParameters.h"

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

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };
    AudioAnalyzer analyzer;
    VisualizerParameterRefs paramRefs;

private:
    // Writes to a real log file (unlike DBG, which is a no-op in Release
    // builds) so shader compile/link failures are diagnosable even outside
    // a debugger -- see VisualizerRenderer's compileProgram().
    std::unique_ptr<juce::FileLogger> fileLogger;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KaleidosonicAudioProcessor)
};
