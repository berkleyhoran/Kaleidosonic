#pragma once

#include <JuceHeader.h>
#include <array>
#include <mutex>
#include <random>
#include <vector>
#include "../AudioAnalyzer.h"
#include "../VisualizerParameters.h"
#include "FractalNavigator.h"
#include "PipeNetwork.h"
#include "MazeWalker.h"

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

    // Manual navigation for the "explorer" presets, called from the editor
    // (mouse wheel / arrow keys / drag). Thread-safe: accumulates into
    // atomics that the render thread consumes each frame. Ignored unless
    // the active preset is one of the manual explorers.
    void requestManualZoom(float steps);                 // +steps zooms in, -steps zooms out
    void requestManualPan(float dxFraction, float dyFraction); // fractions of the current view radius

    // Hands a freshly-loaded picture (Load Image... in the editor) to the
    // image-reactive presets. Safe to call from the message thread at any
    // time, including before the GL context exists -- the actual texture
    // upload happens lazily on the GL thread, the first renderOpenGL()
    // after this. Decoding to a raw RGBA buffer happens here, synchronously
    // on the caller's thread (cheap relative to a user's one-off file
    // picker action; keeps the GL thread free of image-decode work).
    void setSourceImage(const juce::Image& image);

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
            feedback, iterations, distortion, zoomWander, cameraShake, cameraScale, palette, prevFrame, waveform,
            fractalOrbit, fractalOrbitLength, fractalRadius, fractalFade, fractalRefOffset, fractalIterNeed,
            ifsZoomScale, ifsFade, zoomPhase, userImage, userImageAspect, userImageLoaded, pipeJoints,
            pipeHuesA, pipeHueE, mazePos, mazeHeading;
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
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> texA, texB, mixAmount, blendMode;
    };

    struct PostUniforms
    {
        explicit PostUniforms(juce::OpenGLShaderProgram& program);
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> raw, history, resolution, time, onset, level, trails,
            blur, noiseAmount, datamosh, bloomIntensity, vignette, chromaticAberration, colorCycleSpeed, pulseDepth,
            posterize, fisheye, trailDirection, flame, shine, gummy, colorOverride, primaryColor, secondaryColor;
    };

    // One escape-time fractal navigator + its GPU reference-orbit texture,
    // bound to a specific preset index (matching PresetNames::all in
    // VisualizerParameters.h). Two autopilot slots (the kaleidoscope dive
    // presets) and five manual "explorer" slots.
    struct FractalSlot
    {
        FractalSlot(int preset, FractalFormula formula, FractalNavigator::Mode mode, double cx, double cy,
                    double startRadius, double panYSignToUse = 1.0)
            : presetIndex(preset), nav(formula, mode, cx, cy, startRadius),
              manual(mode == FractalNavigator::Mode::Manual), panYSign(panYSignToUse)
        {
        }

        int presetIndex;
        FractalNavigator nav;
        bool manual;
        double panYSign; // -1 for the ship-family presets, whose shaders flip the imaginary axis
        GLuint texture = 0;
    };

    void ensureFramebuffersSized(int width, int height);
    void renderPresetToTarget(CompiledPreset& preset, juce::OpenGLFrameBuffer& target, GLuint prevFrameTex,
                               int width, int height, float onsetEnvelope, int presetIndex);
    void bindFullscreenTriangle();
    void setCommonUniforms(CommonUniforms& u, GLuint prevFrameTex, float onsetEnvelope, int presetIndex);
    void updateWaveformTexture();
    void updateNavigators(float dt);
    void uploadOrbitTextureIfDirty(FractalNavigator& nav, GLuint texture);
    FractalSlot* slotForPreset(int presetIndex);

    AudioAnalyzer& analyzer;
    VisualizerParameterRefs& params;

    std::vector<FractalSlot> fractalSlots;
    std::mt19937 navigatorRng { std::random_device {}() };

    // Manual navigation input from the editor, consumed once per frame by
    // updateNavigators() and routed to the active explorer preset's slot.
    std::atomic<float> pendingZoomSteps { 0.0f };
    std::atomic<float> pendingPanX { 0.0f };
    std::atomic<float> pendingPanY { 0.0f };

    // Unbounded, continuously-shrinking zoom scale used by the exactly-
    // self-similar IFS/DE presets (Sierpinski, Apollonian, Julia) for
    // genuinely continuous deep zoom -- updated the same iterative way as
    // FractalNavigator's viewRadius (never recomputed from a growing
    // exponent, which is what would underflow). See updateNavigators()
    // and common.glsl's uIfsZoomScale for why these don't need
    // perturbation theory to go deep, just double-float precision carried
    // through the fold loop itself.
    double ifsZoomScale = 1.0;
    double ifsTimeSinceReset = 0.0;

    // Sawtooth log2-zoom phase in [0,1) for the exactly-self-similar
    // Sierpinski dive (see uZoomPhase in common.glsl): wrapping the
    // *exponent* is what makes that zoom literally infinite with plain
    // float precision.
    double zoomPhase = 0.0;

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

    // User-uploaded image (Load Image... in the editor) for the
    // image-reactive presets. setSourceImage() (message thread) decodes
    // to a raw RGBA buffer and stashes it here under the mutex;
    // uploadUserImageIfDirty() (GL thread, once per frame) picks it up
    // and does the actual glTexImage2D call. This is the only cross-
    // thread handoff in the renderer -- everything else here already
    // runs entirely on the GL thread.
    std::mutex userImageMutex;
    std::vector<juce::uint8> pendingImagePixels;
    int pendingImageWidth = 0, pendingImageHeight = 0;
    bool userImageDirty = false;
    GLuint userImageTexture = 0;
    float userImageAspect = 1.0f;
    bool userImageLoaded = false;
    void uploadUserImageIfDirty();

    // "Pipes" preset state (see PipeNetwork.h) -- grown each frame in
    // updateNavigators() alongside the fractal navigators, uploaded to a
    // small joint-position texture the same way the reference orbits are.
    PipeNetwork pipeNetwork;
    GLuint pipeJointTexture = 0;
    void uploadPipeTextureIfDirty();

    // "Infinite Maze" preset state (see MazeWalker.h) -- advanced each
    // frame alongside the other navigators. No GPU upload needed at all:
    // just the walker's position/heading as plain uniforms, since the
    // maze itself is a pure function of grid coordinates evaluated
    // independently (and identically) on both sides.
    MazeWalker mazeWalker;

    double startTimeMs = 0.0;
    double lastFrameTimeMs = 0.0;
    float onsetEnvelope = 0.0f;

    // per-frame scratch, set once at the top of renderOpenGL() and read by helpers
    float frameTimeSeconds = 0.0f;
    float frameWidth = 0.0f, frameHeight = 0.0f;
};
