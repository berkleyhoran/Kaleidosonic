#include "VisualizerRenderer.h"
#include "../Presets/PresetManager.h"

using namespace juce::gl;

namespace
{
    // Tiny plumbing shaders, not user-facing presets: one samples a single
    // texture (final blit to screen), one cross-fades two textures
    // (compositing preset A / preset B while presetMorph is in motion), and
    // one is the global post-process pass (trails/blur/noise/datamosh).
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

    const char* postFragSource = R"GLSL(
        #version 330 core
        in vec2 vUv;
        out vec4 fragColor;

        uniform sampler2D uRaw;
        uniform sampler2D uHistory;
        uniform vec2 uResolution;
        uniform float uTime;
        uniform float uOnset;
        uniform float uLevel;
        uniform float uTrails;
        uniform float uBlur;
        uniform float uNoiseAmount;
        uniform float uDatamosh;
        uniform float uBloomIntensity;
        uniform float uVignette;
        uniform float uChromaticAberration;
        uniform float uColorCycleSpeed;
        uniform float uPulseDepth;
        uniform float uPosterize;
        uniform float uFisheye;

        float hash(vec2 p)
        {
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
        }

        vec3 rgb2hsv(vec3 c)
        {
            vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
            vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
            vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
            float d = q.x - min(q.w, q.y);
            float e = 1.0e-10;
            return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
        }

        vec3 hsv2rgb(vec3 c)
        {
            vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + k.xyz) * 6.0 - k.www);
            return c.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), c.y);
        }

        void main()
        {
            vec2 uv = vUv;

            // Fisheye: a cheap power-curve radial warp applied once, centrally,
            // to the sampling coordinate itself -- so it affects whatever
            // preset is active without touching per-preset code.
            if (uFisheye > 0.001)
            {
                vec2 centered = uv - 0.5;
                float r = length(centered);
                float theta = atan(centered.y, centered.x);
                float warped = pow(max(r, 0.0001), 1.0 + uFisheye * 1.3);
                uv = clamp(vec2(cos(theta), sin(theta)) * warped + 0.5, 0.0, 1.0);
            }

            vec2 rawUv = uv;
            vec2 historyUv = uv;
            float rgbSplit = 0.0;

            // Datamosh: real P-frame-style corruption, not just jittered
            // squares. Blocks displace along a per-block "motion vector"
            // (biased horizontal, like real codecs) that only changes every
            // blockPeriod seconds, so smears read as continuous drag rather
            // than flicker. Some blocks fully freeze onto stale history --
            // since history already contains previously-frozen content,
            // frozen regions visibly melt across many following frames
            // instead of just one. Onsets slam the strength/freeze chance
            // and add chromatic-aberration channel splitting for a beat-
            // synced glitch burst.
            if (uDatamosh > 0.001)
            {
                float burst = uOnset;
                float blockPeriod = mix(1.4, 0.28, clamp(uDatamosh + burst * 0.6, 0.0, 1.0));
                float epoch = floor(uTime / blockPeriod);

                float cell = mix(36.0, 8.0, clamp(uDatamosh, 0.0, 1.0));
                vec2 block = floor(uv * cell);

                float dirAngle = hash(block * 1.13 + epoch) * 6.28318530718;
                float dirBias = mix(0.35, 1.0, hash(block * 2.7 + epoch));
                vec2 dir = normalize(vec2(cos(dirAngle), sin(dirAngle) * 0.5 + 0.001));
                float strength = (uDatamosh * 0.16 + burst * 0.35) * dirBias;
                vec2 offset = dir * strength;

                historyUv = clamp(uv + offset, 0.0, 1.0);

                float freezeChance = 0.3 + 0.5 * uDatamosh + burst * 0.35;
                float freeze = step(1.0 - freezeChance, hash(block * 1.37 + epoch * 0.7));
                rawUv = mix(uv, historyUv, freeze);

                rgbSplit = uDatamosh * 0.008 + burst * 0.02;
            }

            vec3 raw;
            if (rgbSplit > 0.0001)
            {
                raw.r = texture(uRaw, clamp(rawUv + vec2(rgbSplit, 0.0), 0.0, 1.0)).r;
                raw.g = texture(uRaw, rawUv).g;
                raw.b = texture(uRaw, clamp(rawUv - vec2(rgbSplit, 0.0), 0.0, 1.0)).b;
            }
            else
            {
                raw = texture(uRaw, rawUv).rgb;
            }

            // Standalone chromatic aberration (independent of Datamosh's own
            // beat-burst channel split): a classic lens-like split that
            // grows radially toward the edges of the frame.
            if (uChromaticAberration > 0.001)
            {
                vec2 fromCenter = uv - 0.5;
                vec2 dir = normalize(fromCenter + 1e-5);
                float amount = uChromaticAberration * (0.006 + 0.02 * dot(fromCenter, fromCenter));
                raw.r = mix(raw.r, texture(uRaw, clamp(rawUv + dir * amount, 0.0, 1.0)).r, 0.9);
                raw.b = mix(raw.b, texture(uRaw, clamp(rawUv - dir * amount, 0.0, 1.0)).b, 0.9);
            }

            if (uBlur > 0.001)
            {
                vec2 texel = (1.0 + uBlur * 5.0) / max(uResolution, vec2(1.0));
                vec3 sum = vec3(0.0);
                for (int x = -1; x <= 1; ++x)
                    for (int y = -1; y <= 1; ++y)
                        sum += texture(uRaw, rawUv + vec2(float(x), float(y)) * texel).rgb;
                raw = mix(raw, sum / 9.0, clamp(uBlur, 0.0, 1.0));
            }

            // Always-on soft glow: bright-pass neighbors added back
            // additively so hot edges genuinely bloom, intensity breathing
            // with the audio instead of being a flat constant.
            vec3 bloom = vec3(0.0);
            {
                vec2 texel = 3.0 / max(uResolution, vec2(1.0));
                for (int x = -1; x <= 1; ++x)
                    for (int y = -1; y <= 1; ++y)
                    {
                        vec3 s = texture(uRaw, clamp(rawUv + vec2(float(x), float(y)) * texel, 0.0, 1.0)).rgb;
                        float lum = dot(s, vec3(0.299, 0.587, 0.114));
                        bloom += s * smoothstep(0.35, 0.9, lum);
                    }
                bloom /= 9.0;
            }
            float bloomAmount = (0.4 + uLevel * 0.7 + uOnset * 1.1) * uBloomIntensity;
            raw += bloom * bloomAmount;

            vec3 history = texture(uHistory, clamp(historyUv, vec2(0.002), vec2(0.998))).rgb;

            // Trails: exponential moving average against last frame's fully
            // processed output -- a bounded, standard phosphor-persistence
            // blend, so it can't blow out or decay to black on its own.
            vec3 col = mix(raw, history, clamp(uTrails, 0.0, 0.97));

            if (uNoiseAmount > 0.001)
            {
                float n = hash(uv * uResolution + fract(uTime * 60.0));
                col += (n - 0.5) * uNoiseAmount * (0.6 + uOnset);
            }

            // Always-evolving: slow continuous hue crawl (with a little
            // jump on every beat) plus an audio-tied brightness pulse, so
            // the whole image keeps changing even with every parameter
            // held perfectly still. Both are gentle by default (Color
            // Cycle Speed / Pulse Depth = 1) and fully tunable/disable-able
            // via those two parameters.
            vec3 hsv = rgb2hsv(clamp(col, 0.0, 1.0));
            hsv.x = fract(hsv.x + uTime * 0.009 * uColorCycleSpeed + uOnset * 0.04 * uColorCycleSpeed);
            float pulseRaw = 0.92 + 0.08 * sin(uTime * 0.55) + uLevel * 0.3 + uOnset * 0.3;
            float pulse = 1.0 + (pulseRaw - 1.0) * uPulseDepth;
            hsv.z = clamp(hsv.z * pulse, 0.0, 1.0);
            col = hsv2rgb(hsv);

            if (uVignette > 0.001)
            {
                float vig = smoothstep(0.35, 1.0, length(uv - 0.5) * 1.35);
                col *= 1.0 - uVignette * vig;
            }

            if (uPosterize > 0.001)
            {
                float levels = mix(64.0, 3.0, clamp(uPosterize, 0.0, 1.0));
                col = floor(col * levels + 0.5) / levels;
            }

            fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
        }
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

    // Splits a real C++ double into a (hi, lo) float32 pair for upload as a
    // GLSL double-float value -- hi captures the value to float32
    // precision, lo captures what that rounding dropped, so hi+lo together
    // carry far more precision than either float alone.
    void splitDouble(double v, float& hi, float& lo)
    {
        hi = (float) v;
        lo = (float) (v - (double) hi);
    }

    // Logger::writeToLog (unlike DBG) still works in Release builds, so a
    // shader that fails to compile is diagnosable from Kaleidosonic.log
    // instead of just silently rendering black.
    void logShaderError(const char* stage, const juce::String& error)
    {
        const auto message = juce::String(stage) + " error: " + error;
        DBG(message);
        juce::Logger::writeToLog(message);
    }

    bool compileProgram(juce::OpenGLContext& context, juce::OpenGLShaderProgram& program,
                         const juce::String& vertexSrc, const juce::String& fragmentSrc)
    {
        juce::ignoreUnused(context);
        if (! program.addVertexShader(vertexSrc))
        {
            logShaderError("Vertex shader", program.getLastError());
            return false;
        }
        if (! program.addFragmentShader(fragmentSrc))
        {
            logShaderError("Fragment shader", program.getLastError());
            return false;
        }
        if (! program.link())
        {
            logShaderError("Shader link", program.getLastError());
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
    zoomWander              = makeUniformIfPresent(program, "uZoomWander");
    cameraShake             = makeUniformIfPresent(program, "uCameraShake");
    cameraScale             = makeUniformIfPresent(program, "uCameraScale");
    prevFrame               = makeUniformIfPresent(program, "uPrevFrame");
    waveform                = makeUniformIfPresent(program, "uWaveform");
    fractalRe               = makeUniformIfPresent(program, "uFractalRe");
    fractalIm               = makeUniformIfPresent(program, "uFractalIm");
    fractalRadius           = makeUniformIfPresent(program, "uFractalRadius");
    fractalFade             = makeUniformIfPresent(program, "uFractalFade");
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

VisualizerRenderer::PostUniforms::PostUniforms(juce::OpenGLShaderProgram& program)
{
    raw = makeUniformIfPresent(program, "uRaw");
    history = makeUniformIfPresent(program, "uHistory");
    resolution = makeUniformIfPresent(program, "uResolution");
    time = makeUniformIfPresent(program, "uTime");
    onset = makeUniformIfPresent(program, "uOnset");
    level = makeUniformIfPresent(program, "uLevel");
    trails = makeUniformIfPresent(program, "uTrails");
    blur = makeUniformIfPresent(program, "uBlur");
    noiseAmount = makeUniformIfPresent(program, "uNoiseAmount");
    datamosh = makeUniformIfPresent(program, "uDatamosh");
    bloomIntensity = makeUniformIfPresent(program, "uBloomIntensity");
    vignette = makeUniformIfPresent(program, "uVignette");
    chromaticAberration = makeUniformIfPresent(program, "uChromaticAberration");
    colorCycleSpeed = makeUniformIfPresent(program, "uColorCycleSpeed");
    pulseDepth = makeUniformIfPresent(program, "uPulseDepth");
    posterize = makeUniformIfPresent(program, "uPosterize");
    fisheye = makeUniformIfPresent(program, "uFisheye");
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

    postProgram = std::make_unique<juce::OpenGLShaderProgram>(context);
    if (compileProgram(context, *postProgram, vertexSrc, postFragSource))
        postUniforms = std::make_unique<PostUniforms>(*postProgram);

    presets.clear();
    const int numPresets = PresetManager::getNumPresets();
    for (int i = 0; i < numPresets; ++i)
    {
        CompiledPreset preset;
        preset.program = std::make_unique<juce::OpenGLShaderProgram>(context);
        preset.linked = compileProgram(context, *preset.program, vertexSrc, PresetManager::getFragmentSource(i));
        if (preset.linked)
            preset.uniforms = std::make_unique<CommonUniforms>(*preset.program);
        else
            juce::Logger::writeToLog("Preset " + juce::String(i) + " ("
                                      + PresetNames::all[i] + ") failed to compile -- will render black.");
        presets.push_back(std::move(preset));
    }

    glGenVertexArrays(1, &dummyVAO);

    glGenTextures(1, &waveformTexture);
    glBindTexture(GL_TEXTURE_2D, waveformTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, AudioAnalyzer::waveformSize, 1, 0, GL_RED, GL_FLOAT, nullptr);

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
    postProgram.reset();
    postUniforms.reset();

    historyFBO[0].release();
    historyFBO[1].release();
    scratchFBO.release();
    scratchFBO2.release();
    rawFBO.release();

    if (dummyVAO != 0)
    {
        glDeleteVertexArrays(1, &dummyVAO);
        dummyVAO = 0;
    }

    if (waveformTexture != 0)
    {
        glDeleteTextures(1, &waveformTexture);
        waveformTexture = 0;
    }
}

void VisualizerRenderer::ensureFramebuffersSized(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    if (width == lastWidth && height == lastHeight)
        return;

    juce::OpenGLFrameBuffer* buffers[] = { &historyFBO[0], &historyFBO[1], &scratchFBO, &scratchFBO2, &rawFBO };
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

void VisualizerRenderer::updateNavigators(float dt)
{
    // Deliberately does NOT read Reactivity: that parameter is for color/
    // brightness response, and coupling actual camera/zoom motion to it
    // made fast-reactivity settings genuinely nauseating (rapid zoom
    // jitter). Camera Shake is the dedicated, independent control for how
    // hard the dive speeds up on bass/beats.
    const float zoomSpeedParam = params.zoomSpeed != nullptr ? params.zoomSpeed->load() : 0.3f;
    const float cameraShakeValue = params.cameraShake != nullptr ? params.cameraShake->load() : 1.0f;

    // zoomSpeedParam -1..1 -> a gentle baseline per-second shrink rate.
    // Even at the fastest setting this is a continuous, smooth rate change
    // (not a per-frame snap), which reads as acceleration rather than a jolt.
    const double t = juce::jlimit(0.0f, 1.0f, (zoomSpeedParam + 1.0f) * 0.5f);
    const double baseRate = 0.999 - 0.14 * t;

    const double audioSpeedup = 1.0 + (double) (analyzer.getBassAutoGain() * 0.5f + onsetEnvelope * 0.4f)
                                     * (double) cameraShakeValue;
    const double rate = std::pow(baseRate, audioSpeedup);

    mandelbrotNav.update((double) dt, rate, navigatorRng);
    burningShipNav.update((double) dt, rate, navigatorRng);
}

void VisualizerRenderer::updateWaveformTexture()
{
    analyzer.copyWaveform(waveformSnapshot);
    glBindTexture(GL_TEXTURE_2D, waveformTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AudioAnalyzer::waveformSize, 1, GL_RED, GL_FLOAT,
                     waveformSnapshot.data());
}

void VisualizerRenderer::setCommonUniforms(CommonUniforms& u, GLuint prevFrameTex, float onsetEnv, int presetIndex)
{
    setU(u.resolution, frameWidth, frameHeight);
    setU(u.time, frameTimeSeconds);

    const float reactivityValue = params.reactivity != nullptr ? params.reactivity->load() : 1.0f;
    const float bassGain = params.bassGain != nullptr ? params.bassGain->load() : 1.0f;
    const float midGain = params.midGain != nullptr ? params.midGain->load() : 1.0f;
    const float trebleGain = params.trebleGain != nullptr ? params.trebleGain->load() : 1.0f;

    setU(u.bass, analyzer.getBassAutoGain() * bassGain);
    setU(u.mid, analyzer.getMidAutoGain() * midGain);
    setU(u.treble, analyzer.getTrebleAutoGain() * trebleGain);
    setU(u.level, analyzer.getLevelAutoGain());
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
    setU(u.zoomWander, params.zoomWander != nullptr ? params.zoomWander->load() : 1.0f);
    setU(u.cameraShake, params.cameraShake != nullptr ? params.cameraShake->load() : 1.0f);
    const float cameraScaleValue = params.cameraScale != nullptr ? params.cameraScale->load() : 1.0f;
    setU(u.cameraScale, cameraScaleValue);

    const FractalNavigator& nav = presetIndex == burningShipPresetIndex ? burningShipNav : mandelbrotNav;
    float reHi = 0.0f, reLo = 0.0f, imHi = 0.0f, imLo = 0.0f;
    splitDouble(nav.centerX(), reHi, reLo);
    splitDouble(nav.centerY(), imHi, imLo);
    setU(u.fractalRe, reHi, reLo);
    setU(u.fractalIm, imHi, imLo);
    setU(u.fractalRadius, (float) (nav.radius() * cameraScaleValue));
    setU(u.fractalFade, nav.fadeEnvelope());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prevFrameTex);
    setU(u.prevFrame, (GLint) 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, waveformTexture);
    setU(u.waveform, (GLint) 1);
}

void VisualizerRenderer::renderPresetToTarget(CompiledPreset& preset, juce::OpenGLFrameBuffer& target,
                                               GLuint prevFrameTex, int width, int height, float onsetEnv,
                                               int presetIndex)
{
    target.makeCurrentRenderingTarget();
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    if (preset.linked && preset.program != nullptr)
    {
        preset.program->use();
        bindFullscreenTriangle();
        setCommonUniforms(*preset.uniforms, prevFrameTex, onsetEnv, presetIndex);
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

    updateWaveformTexture();
    updateNavigators(dt);

    const int numPresets = (int) presets.size();
    if (numPresets == 0)
        return;

    const float presetRaw = params.presetIndex != nullptr ? params.presetIndex->load() : 0.0f;
    const int presetA = juce::jlimit(0, numPresets - 1, juce::roundToInt(presetRaw));
    const int presetB = (presetA + 1) % numPresets;
    const float morph = params.presetMorph != nullptr ? juce::jlimit(0.0f, 1.0f, params.presetMorph->load()) : 0.0f;

    juce::OpenGLFrameBuffer& historyWrite = historyFBO[pingIndex];
    juce::OpenGLFrameBuffer& historyRead = historyFBO[1 - pingIndex];
    const GLuint prevFrameTex = historyRead.getTextureID();

    // Stage A: render the selected preset(s) into rawFBO.
    if (morph < 0.001f)
    {
        renderPresetToTarget(presets[(size_t) presetA], rawFBO, prevFrameTex, width, height, onsetEnvelope, presetA);
    }
    else
    {
        renderPresetToTarget(presets[(size_t) presetA], scratchFBO, prevFrameTex, width, height, onsetEnvelope,
                              presetA);
        renderPresetToTarget(presets[(size_t) presetB], scratchFBO2, prevFrameTex, width, height, onsetEnvelope,
                              presetB);

        rawFBO.makeCurrentRenderingTarget();
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
        rawFBO.releaseAsRenderingTarget();
    }

    // Stage B: global post-FX (trails/blur/noise/datamosh), raw + history -> historyWrite.
    historyWrite.makeCurrentRenderingTarget();
    glViewport(0, 0, width, height);
    if (postProgram != nullptr && postUniforms != nullptr)
    {
        postProgram->use();
        bindFullscreenTriangle();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rawFBO.getTextureID());
        setU(postUniforms->raw, (GLint) 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, historyRead.getTextureID());
        setU(postUniforms->history, (GLint) 1);

        setU(postUniforms->resolution, frameWidth, frameHeight);
        setU(postUniforms->time, frameTimeSeconds);
        setU(postUniforms->onset, onsetEnvelope);
        setU(postUniforms->level, analyzer.getLevelAutoGain());
        setU(postUniforms->trails, params.trails != nullptr ? params.trails->load() : 0.0f);
        setU(postUniforms->blur, params.blur != nullptr ? params.blur->load() : 0.0f);
        setU(postUniforms->noiseAmount, params.noiseAmount != nullptr ? params.noiseAmount->load() : 0.0f);
        setU(postUniforms->datamosh, params.datamosh != nullptr ? params.datamosh->load() : 0.0f);
        setU(postUniforms->bloomIntensity, params.bloomIntensity != nullptr ? params.bloomIntensity->load() : 1.0f);
        setU(postUniforms->vignette, params.vignette != nullptr ? params.vignette->load() : 0.0f);
        setU(postUniforms->chromaticAberration,
             params.chromaticAberration != nullptr ? params.chromaticAberration->load() : 0.0f);
        setU(postUniforms->colorCycleSpeed,
             params.colorCycleSpeed != nullptr ? params.colorCycleSpeed->load() : 1.0f);
        setU(postUniforms->pulseDepth, params.pulseDepth != nullptr ? params.pulseDepth->load() : 1.0f);
        setU(postUniforms->posterize, params.posterize != nullptr ? params.posterize->load() : 0.0f);
        setU(postUniforms->fisheye, params.fisheye != nullptr ? params.fisheye->load() : 0.0f);

        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    else
    {
        // Post shader failed to compile: fall back to a straight copy so the
        // preset is still visible instead of a black screen.
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (blitProgram != nullptr && blitUniforms != nullptr)
        {
            blitProgram->use();
            bindFullscreenTriangle();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rawFBO.getTextureID());
            setU(blitUniforms->tex, (GLint) 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }
    historyWrite.releaseAsRenderingTarget();

    // Final blit: this frame's fully processed composite -> the screen.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    if (blitProgram != nullptr && blitUniforms != nullptr)
    {
        blitProgram->use();
        bindFullscreenTriangle();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, historyWrite.getTextureID());
        setU(blitUniforms->tex, (GLint) 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    pingIndex = 1 - pingIndex;
}
