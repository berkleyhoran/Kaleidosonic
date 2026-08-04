#include "PluginProcessor.h"
#include "PluginEditor.h"

KaleidosonicAudioProcessor::KaleidosonicAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    fileLogger.reset(juce::FileLogger::createDefaultAppLogger("Kaleidosonic", "Kaleidosonic.log",
                                                                "Kaleidosonic log started"));
    juce::Logger::setCurrentLogger(fileLogger.get());

    paramRefs.resolve(apvts);
}

KaleidosonicAudioProcessor::~KaleidosonicAudioProcessor()
{
    juce::Logger::setCurrentLogger(nullptr);
}

bool KaleidosonicAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    return mainOut == layouts.getMainInputChannelSet();
}

void KaleidosonicAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    analyzer.prepare(sampleRate, samplesPerBlock);
}

void KaleidosonicAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    analyzer.pushBlock(buffer);
    // Pure visualizer: audio passes through unmodified.
}

juce::AudioProcessorEditor* KaleidosonicAudioProcessor::createEditor()
{
    return new KaleidosonicAudioProcessorEditor(*this);
}

void KaleidosonicAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void KaleidosonicAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KaleidosonicAudioProcessor();
}
