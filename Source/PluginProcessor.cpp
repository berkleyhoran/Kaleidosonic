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
#if JUCE_WINDOWS
    lastPreparedSampleRate = sampleRate;
    lastPreparedSamplesPerBlock = samplesPerBlock;
    if (! systemAudioCaptureEnabled)
        analyzer.prepare(sampleRate, samplesPerBlock);
    // else: leave the analyzer prepared at the loopback device's own rate
    // (set by SystemAudioLoopbackCapture::run()) -- re-preparing here to
    // this device's rate on every device-manager hiccup would just fight
    // the capture thread.
#else
    analyzer.prepare(sampleRate, samplesPerBlock);
#endif
}

void KaleidosonicAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
#if JUCE_WINDOWS
    if (! systemAudioCaptureEnabled)
        analyzer.pushBlock(buffer);
    // else: SystemAudioLoopbackCapture is pushing blocks directly from its
    // own thread -- feeding this device's own input too would just mix
    // two unrelated signals into the same FFT.
#else
    analyzer.pushBlock(buffer);
#endif
    // Pure visualizer: audio passes through unmodified.
}

#if JUCE_WINDOWS
void KaleidosonicAudioProcessor::setSystemAudioCaptureEnabled(bool enabled)
{
    // Standalone-only, checked at *runtime* rather than with
    // `#if JucePlugin_Build_Standalone`: this project's JUCE/CMake setup
    // compiles the shared plugin code (this file) exactly once and reuses
    // the same object files across every format's final binary, so that
    // macro is baked in as whichever value happened to apply to the build
    // that first compiled it -- NOT the format actually being run. (Easy
    // to verify: search either built binary for a Standalone-only string
    // and it shows up in both. Learned that the hard way here.)
    // AudioProcessor::wrapperType is instead set at *construction* time by
    // whichever wrapper is actually hosting this specific instance, so
    // it's the one reliable way to ask "am I really running standalone
    // right now" when the surrounding code can't be trusted to differ per
    // format.
    if (wrapperType != juce::AudioProcessor::wrapperType_Standalone)
    {
        juce::Logger::writeToLog("Kaleidosonic: system audio capture is Standalone-only, ignoring request "
                                  "from wrapperType=" + juce::String((int) wrapperType));
        return;
    }

    if (enabled == systemAudioCaptureEnabled)
        return;

    if (enabled)
    {
        if (systemAudioCapture == nullptr)
            systemAudioCapture = std::make_unique<SystemAudioLoopbackCapture>();

        if (systemAudioCapture->start(analyzer))
        {
            systemAudioCaptureEnabled = true;
        }
        else
        {
            juce::Logger::writeToLog(
                "Kaleidosonic: system audio capture failed to start (see the line above for why) "
                "-- falling back to the normal audio input");
        }
    }
    else
    {
        systemAudioCapture->stop();
        systemAudioCaptureEnabled = false;
        analyzer.prepare(lastPreparedSampleRate, lastPreparedSamplesPerBlock);
    }
}
#endif

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
