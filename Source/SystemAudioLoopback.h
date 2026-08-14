#pragma once

#include <JuceHeader.h>

#if JUCE_WINDOWS

class AudioAnalyzer;

// Captures whatever the system's current default output device is
// playing (Windows WASAPI *loopback*) and feeds it straight into an
// AudioAnalyzer -- lets the Standalone app visualize "what your speakers
// are outputting" (Spotify, a browser tab, another app entirely) without
// routing audio through a virtual cable (VB-Cable, VoiceMeeter, etc.)
// first. Only meaningful for the Standalone app: a VST3 instance already
// gets its host track's audio directly through the normal plugin audio
// path, so there's nothing to loop back for it -- see the
// `JucePlugin_Build_Standalone && JUCE_WINDOWS` guards around every call
// site of this class.
//
// JUCE's own WASAPI backend only exposes shared/exclusive/sharedLowLatency
// modes (see WASAPIDeviceMode) -- no loopback -- so this talks to WASAPI
// directly via the same COM interfaces JUCE's backend uses internally
// (IMMDeviceEnumerator/IAudioClient/IAudioCaptureClient), just with
// AUDCLNT_STREAMFLAGS_LOOPBACK set on the default *render* endpoint
// (Microsoft's documented "Loopback Recording" pattern -- capturing a
// render endpoint's own mix instead of opening a capture endpoint).
//
// Every WASAPI/COM call happens on one dedicated MTA thread (opened,
// polled, and closed all in run()) -- these COM interfaces are picky
// about being used consistently from a single apartment, so nothing here
// ever touches them from the message thread.
class SystemAudioLoopbackCapture : private juce::Thread
{
public:
    SystemAudioLoopbackCapture();
    ~SystemAudioLoopbackCapture() override;

    // Starts capturing the default output device into `analyzerToUse`,
    // re-preparing it at the loopback stream's real sample rate (almost
    // certainly different from whatever the app's own audio device was
    // prepared with). Blocks briefly (a few seconds at most) so the
    // caller gets a real success/failure result -- e.g. false if there's
    // no default output device -- instead of an optimistic guess; the
    // reason is always logged either way.
    bool start(AudioAnalyzer& analyzerToUse);

    // Stops capturing and joins the thread. Does NOT restore the
    // analyzer to any previous prepared state -- that's the caller's job
    // (it's the caller who knows what "previous" should mean).
    void stop();

private:
    void run() override;

    AudioAnalyzer* analyzer = nullptr;
    juce::WaitableEvent openResultReady;
    std::atomic<bool> openSucceeded { false };

    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemAudioLoopbackCapture)
};

#endif // JUCE_WINDOWS
