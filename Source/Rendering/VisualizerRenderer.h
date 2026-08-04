#pragma once

#include <JuceHeader.h>
#include <array>
#include <random>
#include "../AudioAnalyzer.h"
#include "../VisualizerParameters.h"
#include "FractalNavigator.h"

// Owns the OpenGLContext and does all GL work. Each frame:
//   1. the current (and, while morphing, the next) preset renders into an
//      offscreen "raw" buffer;
//   2. a post-process pass blends that against last frame's fully-processed
//      output using the global Trails/Blur/Noise/Datamosh parameters, and
//      writes the result into a ping-pong history buffer;
//   3. that history buffer is blitted to screen, and also serves as
//      uPrevFrame for feedback-style presets and as next frame's "history"
//      input to step 2.
//
// All the interesting per-frame numbers (audio bands, onset, every
// automatable parameter) are read from lock-free atomics owned by the
// processor, so this class never touches the audio thread directly.
class VisualizerRenderer : public juce::OpenGLRenderer
{
public:
    VisualizerRenderer(AudioAnalyzer& analyzerToUse, VisualizerParameterRefs& paramsToUse);
    ~VisualizerRenderer() override;

    void attachTo(juce::Component& component);
    void detach();

    // juce::OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    struct CommonUniforms
    {
        explicit CommonUniforms(juce::OpenGLShaderProgram& program);

        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> resolution, time, bass, mid, treble, level, onset,
            reactivity, zoomSpeed, rotationSpeed, hue, saturation, brightness, contrast, kaleidoscopeSegments,
            feedback, iterations, distortion, zoomWander, cameraShake, cameraScale, prevFrame, waveform,
            fractalRe, fractalIm, fractalRadius, fractalFade;
    };

    struct CompiledPreset
    {
        std::unique_ptr<juce::OpenGLShaderProgram> program;
        std::unique_ptr<CommonUniforms> uniforms;
        bool linked = false;
    };

    struct BlitUniforms
    {
        explicit BlitUniforms(juce::OpenGLShaderProgram& program);
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> tex;
    };

    struct BlendUniforms
    {
        explicit BlendUniforms(juce::OpenGLShaderProgram& program);
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> texA, texB, mixAmount;
    };

    struct PostUniforms
    {
        explicit PostUniforms(juce::OpenGLShaderProgram& program);
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> raw, history, resolution, time, onset, level, trails,
            blur, noiseAmount, datamosh, bloomIntensity, vignette, chromaticAberration, colorCycleSpeed, pulseDepth,
            posterize, fisheye;
    };

    void ensureFramebuffersSized(int width, int height);
    void renderPresetToTarget(CompiledPreset& preset, juce::OpenGLFrameBuffer& target, GLuint prevFrameTex,
                               int width, int height, float onsetEnvelope, int presetIndex);
    void bindFullscreenTriangle();
    void setCommonUniforms(CommonUniforms& u, GLuint prevFrameTex, float onsetEnvelope, int presetIndex);
    void updateWaveformTexture();
    void updateNavigators(float dt);

    AudioAnalyzer& analyzer;
    VisualizerParameterRefs& params;

    // Distance-estimator-guided autopilots (real C++ double precision) that
    // decide where Mandelbrot Pulse / Burning Ship zoom each frame -- see
    // FractalNavigator.h. Indices must match PresetNames::all in
    // VisualizerParameters.h.
    static constexpr int mandelbrotPresetIndex = 0;
    static constexpr int burningShipPresetIndex = 5;
    FractalNavigator mandelbrotNav { -0.745, 0.11, false };
    FractalNavigator burningShipNav { -1.75, -0.03, true };
    std::mt19937 navigatorRng { std::random_device {}() };

    juce::OpenGLContext context;
    juce::Component* attachedComponent = nullptr;

    std::vector<CompiledPreset> presets;

    std::unique_ptr<juce::OpenGLShaderProgram> blitProgram;
    std::unique_ptr<BlitUniforms> blitUniforms;
    std::unique_ptr<juce::OpenGLShaderProgram> blendProgram;
    std::unique_ptr<BlendUniforms> blendUniforms;
    std::unique_ptr<juce::OpenGLShaderProgram> postProgram;
    std::unique_ptr<PostUniforms> postUniforms;

    juce::OpenGLFrameBuffer historyFBO[2];
    juce::OpenGLFrameBuffer scratchFBO, scratchFBO2;
    juce::OpenGLFrameBuffer rawFBO;
    int pingIndex = 0;
    int lastWidth = 0, lastHeight = 0;

    GLuint dummyVAO = 0;
    GLuint waveformTexture = 0;
    std::array<float, (size_t) AudioAnalyzer::waveformSize> waveformSnapshot {};

    double startTimeMs = 0.0;
    double lastFrameTimeMs = 0.0;
    float onsetEnvelope = 0.0f;

    // per-frame scratch, set once at the top of renderOpenGL() and read by helpers
    float frameTimeSeconds = 0.0f;
    float frameWidth = 0.0f, frameHeight = 0.0f;
};
