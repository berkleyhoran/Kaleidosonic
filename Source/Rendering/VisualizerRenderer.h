#pragma once

#include <JuceHeader.h>
#include "../AudioAnalyzer.h"
#include "../VisualizerParameters.h"

// Owns the OpenGLContext and does all GL work: compiles every preset shader
// once up front, renders the current (and, while morphing, the next) preset
// into an offscreen ping-pong buffer so feedback-style presets can sample
// the previous composited frame, then blits the result to screen.
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
            feedback, iterations, distortion, prevFrame;
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

    void ensureFramebuffersSized(int width, int height);
    void renderPresetToTarget(CompiledPreset& preset, juce::OpenGLFrameBuffer& target, GLuint prevFrameTex,
                               int width, int height, float onsetEnvelope);
    void bindFullscreenTriangle();
    void setCommonUniforms(CommonUniforms& u, GLuint prevFrameTex, float onsetEnvelope);

    AudioAnalyzer& analyzer;
    VisualizerParameterRefs& params;

    juce::OpenGLContext context;
    juce::Component* attachedComponent = nullptr;

    std::vector<CompiledPreset> presets;

    std::unique_ptr<juce::OpenGLShaderProgram> blitProgram;
    std::unique_ptr<BlitUniforms> blitUniforms;
    std::unique_ptr<juce::OpenGLShaderProgram> blendProgram;
    std::unique_ptr<BlendUniforms> blendUniforms;

    juce::OpenGLFrameBuffer historyFBO[2];
    juce::OpenGLFrameBuffer scratchFBO, scratchFBO2;
    int pingIndex = 0;
    int lastWidth = 0, lastHeight = 0;

    GLuint dummyVAO = 0;

    double startTimeMs = 0.0;
    double lastFrameTimeMs = 0.0;
    float onsetEnvelope = 0.0f;

    // per-frame scratch, set once at the top of renderOpenGL() and read by helpers
    float frameTimeSeconds = 0.0f;
    float frameWidth = 0.0f, frameHeight = 0.0f;
};
