#include "PluginProcessor.h"
#include "PluginEditor.h"

#if JUCE_WINDOWS
// Only used from setSystemAudioCaptureEnabled() below, itself already
// gated to real Standalone instances at *runtime* via wrapperType (see
// that function's own comment) -- StandalonePluginHolder is entirely
// header-only/inline (no separate .cpp providing its definitions), so
// including it here is safe to compile into every format's shared code
// object the same way the rest of this file already is: in a VST3/AU
// build getInstance() simply returns nullptr (nothing ever constructs one
// there), guarded below anyway.
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

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

    // Updated unconditionally, even in system-audio-loopback mode where
    // this thread never calls pushBlock() itself -- just an atomic store,
    // and it's what SystemAudioLoopbackCapture's own pushBlock() call (from
    // its own thread) picks up next time it runs.
    analyzer.setEqGains(paramRefs.eqLowGain != nullptr ? paramRefs.eqLowGain->load() : 0.0f,
                         paramRefs.eqMidGain != nullptr ? paramRefs.eqMidGain->load() : 0.0f,
                         paramRefs.eqHighGain != nullptr ? paramRefs.eqHighGain->load() : 0.0f);

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

            // JUCE's built-in Standalone window shows an "Audio input is
            // muted to avoid feedback loop" banner whenever the normal
            // input is muted -- which it is by default, since this
            // processor declares both an input and output bus. That
            // warning is about the normal mic/line input, which isn't
            // even being read right now: pushBlock() only ever gets fed
            // from the WASAPI loopback thread while capture is on (see
            // processBlock()'s guard above), so the banner is just noise
            // in this mode. Auto-unmuting suppresses it.
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                holder->getMuteInputValue().setValue(false);
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

        // Restore the normal muted-by-default state now that the real
        // input is what's actually feeding the analyzer again -- the
        // feedback-loop warning is legitimate once more.
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->getMuteInputValue().setValue(true);
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
