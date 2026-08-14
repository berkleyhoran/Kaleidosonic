#include "SystemAudioLoopback.h"

#if JUCE_WINDOWS

#include "AudioAnalyzer.h"

#define WIN32_LEAN_AND_MEAN
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Real COM interface pointers/format live here, not in the header, so
// nothing elsewhere in the project needs to see Windows COM headers just
// because it includes SystemAudioLoopback.h.
struct SystemAudioLoopbackCapture::Impl
{
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioCaptureClient> captureClient;
    WAVEFORMATEX* mixFormat = nullptr;
    bool comInitialized = false;

    ~Impl()
    {
        if (audioClient != nullptr)
            audioClient->Stop();
        if (mixFormat != nullptr)
            CoTaskMemFree(mixFormat);
        audioClient.Reset();
        captureClient.Reset();
        device.Reset();
        if (comInitialized)
            CoUninitialize();
    }
};

SystemAudioLoopbackCapture::SystemAudioLoopbackCapture()
    : juce::Thread("Kaleidosonic System Audio Loopback")
{
}

SystemAudioLoopbackCapture::~SystemAudioLoopbackCapture()
{
    stop();
}

bool SystemAudioLoopbackCapture::start(AudioAnalyzer& analyzerToUse)
{
    stop();
    analyzer = &analyzerToUse;
    openResultReady.reset();
    openSucceeded = false;
    startThread(juce::Thread::Priority::high);

    // run() opens the device (or fails) within a few ms in practice; the
    // 3s bound is just a generous safety margin against a genuinely stuck
    // driver rather than an expected wait.
    const bool signalled = openResultReady.wait(3000);
    return signalled && openSucceeded.load();
}

void SystemAudioLoopbackCapture::stop()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        stopThread(2000);
    }
    analyzer = nullptr;
}

void SystemAudioLoopbackCapture::run()
{
    impl = std::make_unique<Impl>();

    auto fail = [this](const char* what, HRESULT hr)
    {
        juce::Logger::writeToLog("SystemAudioLoopbackCapture: " + juce::String(what)
                                  + " failed, hr=0x" + juce::String::toHexString((int) hr));
        impl.reset();
        openSucceeded = false;
        openResultReady.signal();
    };

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    impl->comInitialized = SUCCEEDED(hr);

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                           (void**) enumerator.GetAddressOf());
    if (FAILED(hr))
        return fail("CoCreateInstance(MMDeviceEnumerator)", hr);

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, impl->device.GetAddressOf());
    if (FAILED(hr))
        return fail("GetDefaultAudioEndpoint (no default output device?)", hr);

    hr = impl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 (void**) impl->audioClient.GetAddressOf());
    if (FAILED(hr))
        return fail("IAudioClient Activate", hr);

    hr = impl->audioClient->GetMixFormat(&impl->mixFormat);
    if (FAILED(hr) || impl->mixFormat == nullptr)
        return fail("GetMixFormat", hr);

    // WASAPI's shared-mode mix format is always float in practice (it's
    // the format the OS's own mixer works in internally) -- both plain
    // WAVE_FORMAT_IEEE_FLOAT and the WAVE_FORMAT_EXTENSIBLE wrapper around
    // it count; anything else would be a genuinely unusual driver.
    const bool isFloat = impl->mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT
                        || impl->mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE;
    if (! isFloat)
        return fail("mix format isn't float", E_NOTIMPL);

    constexpr REFERENCE_TIME bufferDuration = 200000; // 20ms in 100ns units
    hr = impl->audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0,
                                        impl->mixFormat, nullptr);
    if (FAILED(hr))
        return fail("Initialize(loopback)", hr);

    hr = impl->audioClient->GetService(__uuidof(IAudioCaptureClient), (void**) impl->captureClient.GetAddressOf());
    if (FAILED(hr))
        return fail("GetService(IAudioCaptureClient)", hr);

    const int channels = (int) impl->mixFormat->nChannels;
    const double sampleRate = (double) impl->mixFormat->nSamplesPerSec;
    analyzer->prepare(sampleRate, 512);

    hr = impl->audioClient->Start();
    if (FAILED(hr))
        return fail("IAudioClient Start", hr);

    openSucceeded = true;
    openResultReady.signal();

    juce::AudioBuffer<float> scratch(channels, 4096);

    while (! threadShouldExit())
    {
        juce::Thread::sleep(10);

        UINT32 packetLength = 0;
        if (FAILED(impl->captureClient->GetNextPacketSize(&packetLength)))
            break;

        while (packetLength != 0)
        {
            BYTE* data = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;
            if (FAILED(impl->captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr)))
                break;

            if (numFrames > 0)
            {
                scratch.setSize(channels, (int) numFrames, false, false, true);
                const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                const auto* interleaved = reinterpret_cast<const float*>(data);

                for (int ch = 0; ch < channels; ++ch)
                {
                    float* dest = scratch.getWritePointer(ch);
                    if (silent)
                        juce::FloatVectorOperations::clear(dest, (int) numFrames);
                    else
                        for (UINT32 i = 0; i < numFrames; ++i)
                            dest[i] = interleaved[i * (UINT32) channels + (UINT32) ch];
                }

                analyzer->pushBlock(scratch);
            }

            impl->captureClient->ReleaseBuffer(numFrames);

            if (FAILED(impl->captureClient->GetNextPacketSize(&packetLength)))
                break;
        }
    }

    impl.reset();
}

#endif // JUCE_WINDOWS
