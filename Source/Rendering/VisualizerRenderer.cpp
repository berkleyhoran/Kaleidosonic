#include "VisualizerRenderer.h"
#include "../Presets/PresetManager.h"

using namespace juce::gl;

namespace
{
    // Tiny plumbing shaders, not user-facing presets: one samples a single
    // texture (final blit to screen), the other cross-fades two textures
    // (compositing preset A / preset B while presetMorph is in motion).
    const char* blitFragSource = R"GLSL(
        #version 330 core
        in vec2 vUv;
        out vec4 fragColor;
        uniform sampler2D uTex;
        void main() { fragColor = texture(uTex, vUv); }
    )GLSL";

    const char* blendFragSource = R"GLSL(
        #version 330 core
        in vec2 vUv;
        out vec4 fragColor;
        uniform sampler2D uTexA;
        uniform sampler2D uTexB;
        uniform float uMix;
        void main() { fragColor = mix(texture(uTexA, vUv), texture(uTexB, vUv), uMix); }
    )GLSL";

    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> makeUniformIfPresent(juce::OpenGLShaderProgram& program,
                                                                              const char* name)
    {
        if (glGetUniformLocation(program.getProgramID(), name) < 0)
            return nullptr;
        return std::make_unique<juce::OpenGLShaderProgram::Uniform>(program, name);
    }

    template <typename... Args>
    void setU(const std::unique_ptr<juce::OpenGLShaderProgram::Uniform>& u, Args... args)
    {
        if (u != nullptr)
            u->set(args...);
    }

    bool compileProgram(juce::OpenGLContext& context, juce::OpenGLShaderProgram& program,
                         const juce::String& vertexSrc, const juce::String& fragmentSrc)
    {
        juce::ignoreUnused(context);
        if (! program.addVertexShader(vertexSrc))
        {
            DBG("Vertex shader error: " << program.getLastError());
            return false;
        }
        if (! program.addFragmentShader(fragmentSrc))
        {
            DBG("Fragment shader error: " << program.getLastError());
            return false;
        }
        if (! program.link())
        {
            DBG("Shader link error: " << program.getLastError());
            return false;
        }
        return true;
    }
}

VisualizerRenderer::CommonUniforms::CommonUniforms(juce::OpenGLShaderProgram& program)
{
    resolution           = makeUniformIfPresent(program, "uResolution");
    time                  = makeUniformIfPresent(program, "uTime");
    bass                  = makeUniformIfPresent(program, "uBass");
    mid                    = makeUniformIfPresent(program, "uMid");
    treble                 = makeUniformIfPresent(program, "uTreble");
    level                  = makeUniformIfPresent(program, "uLevel");
    onset                  = makeUniformIfPresent(program, "uOnset");
    reactivity             = makeUniformIfPresent(program, "uReactivity");
    zoomSpeed               = makeUniformIfPresent(program, "uZoomSpeed");
    rotationSpeed           = makeUniformIfPresent(program, "uRotationSpeed");
    hue                     = makeUniformIfPresent(program, "uHue");
    saturation              = makeUniformIfPresent(program, "uSaturation");
    brightness              = makeUniformIfPresent(program, "uBrightness");
    contrast                = makeUniformIfPresent(program, "uContrast");
    kaleidoscopeSegments    = makeUniformIfPresent(program, "uKaleidoscopeSegments");
    feedback                = makeUniformIfPresent(program, "uFeedback");
    iterations              = makeUniformIfPresent(program, "uIterations");
    distortion              = makeUniformIfPresent(program, "uDistortion");
    prevFrame               = makeUniformIfPresent(program, "uPrevFrame");
}

VisualizerRenderer::BlitUniforms::BlitUniforms(juce::OpenGLShaderProgram& program)
{
    tex = makeUniformIfPresent(program, "uTex");
}

VisualizerRenderer::BlendUniforms::BlendUniforms(juce::OpenGLShaderProgram& program)
{
    texA = makeUniformIfPresent(program, "uTexA");
    texB = makeUniformIfPresent(program, "uTexB");
    mixAmount = makeUniformIfPresent(program, "uMix");
}

VisualizerRenderer::VisualizerRenderer(AudioAnalyzer& analyzerToUse, VisualizerParameterRefs& paramsToUse)
    : analyzer(analyzerToUse), params(paramsToUse)
{
}

VisualizerRenderer::~VisualizerRenderer()
{
    if (context.isAttached())
        context.detach();
}

void VisualizerRenderer::attachTo(juce::Component& component)
{
    attachedComponent = &component;
    context.setOpenGLVersionRequired(juce::OpenGLContext::OpenGLVersion::openGL3_2);
    context.setMultisamplingEnabled(true);
    context.setRenderer(this);
    context.setContinuousRepainting(true);
    context.attachTo(component);
}

void VisualizerRenderer::detach()
{
    context.detach();
    attachedComponent = nullptr;
}

void VisualizerRenderer::newOpenGLContextCreated()
{
    const auto vertexSrc = PresetManager::getVertexSource();

    blitProgram = std::make_unique<juce::OpenGLShaderProgram>(context);
    if (compileProgram(context, *blitProgram, vertexSrc, blitFragSource))
        blitUniforms = std::make_unique<BlitUniforms>(*blitProgram);

    blendProgram = std::make_unique<juce::OpenGLShaderProgram>(context);
    if (compileProgram(context, *blendProgram, vertexSrc, blendFragSource))
        blendUniforms = std::make_unique<BlendUniforms>(*blendProgram);

    presets.clear();
    const int numPresets = PresetManager::getNumPresets();
    for (int i = 0; i < numPresets; ++i)
    {
        CompiledPreset preset;
        preset.program = std::make_unique<juce::OpenGLShaderProgram>(context);
        preset.linked = compileProgram(context, *preset.program, vertexSrc, PresetManager::getFragmentSource(i));
        if (preset.linked)
            preset.uniforms = std::make_unique<CommonUniforms>(*preset.program);
        presets.push_back(std::move(preset));
    }

    glGenVertexArrays(1, &dummyVAO);

    lastWidth = lastHeight = 0;
    startTimeMs = juce::Time::getMillisecondCounterHiRes();
    lastFrameTimeMs = startTimeMs;
    onsetEnvelope = 0.0f;
}

void VisualizerRenderer::openGLContextClosing()
{
    presets.clear();
    blitProgram.reset();
    blitUniforms.reset();
    blendProgram.reset();
    blendUniforms.reset();

    historyFBO[0].release();
    historyFBO[1].release();
    scratchFBO.release();
    scratchFBO2.release();

    if (dummyVAO != 0)
    {
        glDeleteVertexArrays(1, &dummyVAO);
        dummyVAO = 0;
    }
}

void VisualizerRenderer::ensureFramebuffersSized(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    if (width == lastWidth && height == lastHeight)
        return;

    juce::OpenGLFrameBuffer* buffers[] = { &historyFBO[0], &historyFBO[1], &scratchFBO, &scratchFBO2 };
    for (auto* fb : buffers)
    {
        fb->release();
        fb->initialise(context, width, height);
        fb->makeCurrentRenderingTarget();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        fb->releaseAsRenderingTarget();
    }

    lastWidth = width;
    lastHeight = height;
}

void VisualizerRenderer::bindFullscreenTriangle()
{
    glBindVertexArray(dummyVAO);
}

void VisualizerRenderer::setCommonUniforms(CommonUniforms& u, GLuint prevFrameTex, float onsetEnv)
{
    setU(u.resolution, frameWidth, frameHeight);
    setU(u.time, frameTimeSeconds);

    const float reactivityValue = params.reactivity != nullptr ? params.reactivity->load() : 1.0f;
    const float bassGain = params.bassGain != nullptr ? params.bassGain->load() : 1.0f;
    const float midGain = params.midGain != nullptr ? params.midGain->load() : 1.0f;
    const float trebleGain = params.trebleGain != nullptr ? params.trebleGain->load() : 1.0f;

    setU(u.bass, analyzer.getBass() * bassGain);
    setU(u.mid, analyzer.getMid() * midGain);
    setU(u.treble, analyzer.getTreble() * trebleGain);
    setU(u.level, analyzer.getLevel());
    setU(u.onset, onsetEnv);
    setU(u.reactivity, reactivityValue);

    setU(u.zoomSpeed, params.zoomSpeed != nullptr ? params.zoomSpeed->load() : 0.3f);
    setU(u.rotationSpeed, params.rotationSpeed != nullptr ? params.rotationSpeed->load() : 0.2f);
    setU(u.hue, params.hue != nullptr ? params.hue->load() : 0.0f);
    setU(u.saturation, params.saturation != nullptr ? params.saturation->load() : 0.8f);
    setU(u.brightness, params.brightness != nullptr ? params.brightness->load() : 1.0f);
    setU(u.contrast, params.contrast != nullptr ? params.contrast->load() : 1.0f);
    setU(u.kaleidoscopeSegments, params.kaleidoscopeSegments != nullptr ? params.kaleidoscopeSegments->load() : 6.0f);
    setU(u.feedback, params.feedbackAmount != nullptr ? params.feedbackAmount->load() : 0.9f);
    setU(u.iterations, params.iterations != nullptr ? params.iterations->load() : 24.0f);
    setU(u.distortion, params.distortion != nullptr ? params.distortion->load() : 0.3f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prevFrameTex);
    setU(u.prevFrame, (GLint) 0);
}

void VisualizerRenderer::renderPresetToTarget(CompiledPreset& preset, juce::OpenGLFrameBuffer& target,
                                               GLuint prevFrameTex, int width, int height, float onsetEnv)
{
    target.makeCurrentRenderingTarget();
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    if (preset.linked && preset.program != nullptr)
    {
        preset.program->use();
        bindFullscreenTriangle();
        setCommonUniforms(*preset.uniforms, prevFrameTex, onsetEnv);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    else
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    target.releaseAsRenderingTarget();
}

void VisualizerRenderer::renderOpenGL()
{
    if (attachedComponent == nullptr)
        return;

    const double scale = context.getRenderingScale();
    const int width = juce::roundToInt((double) attachedComponent->getWidth() * scale);
    const int height = juce::roundToInt((double) attachedComponent->getHeight() * scale);
    if (width <= 0 || height <= 0)
        return;

    ensureFramebuffersSized(width, height);
    frameWidth = (float) width;
    frameHeight = (float) height;

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit(0.0, 0.25, (now - lastFrameTimeMs) / 1000.0);
    lastFrameTimeMs = now;
    frameTimeSeconds = std::fmod((float) ((now - startTimeMs) / 1000.0), 1000.0f);

    const float onsetPulse = analyzer.consumeOnsetPulse();
    const float decay = std::exp(-dt / 0.12f);
    onsetEnvelope = std::max(onsetEnvelope * decay, onsetPulse);

    const int numPresets = (int) presets.size();
    if (numPresets == 0)
        return;

    const float presetRaw = params.presetIndex != nullptr ? params.presetIndex->load() : 0.0f;
    const int presetA = juce::jlimit(0, numPresets - 1, juce::roundToInt(presetRaw));
    const int presetB = (presetA + 1) % numPresets;
    const float morph = params.presetMorph != nullptr ? juce::jlimit(0.0f, 1.0f, params.presetMorph->load()) : 0.0f;

    juce::OpenGLFrameBuffer& writeTarget = historyFBO[pingIndex];
    juce::OpenGLFrameBuffer& readTarget = historyFBO[1 - pingIndex];
    const GLuint prevFrameTex = readTarget.getTextureID();

    if (morph < 0.001f)
    {
        renderPresetToTarget(presets[(size_t) presetA], writeTarget, prevFrameTex, width, height, onsetEnvelope);
    }
    else
    {
        renderPresetToTarget(presets[(size_t) presetA], scratchFBO, prevFrameTex, width, height, onsetEnvelope);
        renderPresetToTarget(presets[(size_t) presetB], scratchFBO2, prevFrameTex, width, height, onsetEnvelope);

        writeTarget.makeCurrentRenderingTarget();
        glViewport(0, 0, width, height);
        if (blendProgram != nullptr && blendUniforms != nullptr)
        {
            blendProgram->use();
            bindFullscreenTriangle();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, scratchFBO.getTextureID());
            setU(blendUniforms->texA, (GLint) 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, scratchFBO2.getTextureID());
            setU(blendUniforms->texB, (GLint) 1);
            setU(blendUniforms->mixAmount, morph);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        writeTarget.releaseAsRenderingTarget();
    }

    // Final blit: our offscreen composite -> the screen.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    if (blitProgram != nullptr && blitUniforms != nullptr)
    {
        blitProgram->use();
        bindFullscreenTriangle();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, writeTarget.getTextureID());
        setU(blitUniforms->tex, (GLint) 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    pingIndex = 1 - pingIndex;
}
