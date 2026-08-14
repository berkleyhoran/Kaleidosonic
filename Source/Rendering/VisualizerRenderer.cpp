#include "VisualizerRenderer.h"
#include "../Presets/PresetManager.h"
#include "DoubleDouble.h"

using namespace juce::gl;

namespace
{
    // Tiny plumbing shaders, not user-facing presets: one samples a single
    // texture (final blit to screen), one composites Layer A / Layer B
    // (two independently-chosen presets) while layerMix is above zero, and
    // one is the global post-process pass (trails/blur/noise/datamosh).
    const char* blitFragSource = R"GLSL(
        #version 330 core
        in vec2 vUv;
        out vec4 fragColor;
        uniform sampler2D uTex;
        void main() { fragColor = texture(uTex, vUv); }
    )GLSL";

    // uBlendMode picks *how* Layer B combines with Layer A before uMix
    // dissolves between "just A" and that combination -- matches
    // BlendModeNames::all's order in VisualizerParameters.h. Crossfade's
    // "blended" is simply B, so mix(a, b, t) reproduces the old
    // (pre-Phase-4) plain-crossfade behavior exactly when Blend Mode is
    // left at its default.
    const char* blendFragSource = R"GLSL(
        #version 330 core
        in vec2 vUv;
        out vec4 fragColor;
        uniform sampler2D uTexA;
        uniform sampler2D uTexB;
        uniform float uMix;
        uniform float uBlendMode;
        void main()
        {
            vec3 a = texture(uTexA, vUv).rgb;
            vec3 b = texture(uTexB, vUv).rgb;
            int mode = int(uBlendMode + 0.5);
            vec3 blended;
            if (mode == 1) blended = a + b;                              // Add
            else if (mode == 2) blended = 1.0 - (1.0 - a) * (1.0 - b);   // Screen
            else if (mode == 3) blended = a * b;                         // Multiply
            else if (mode == 4) blended = abs(a - b);                    // Difference
            else if (mode == 5) blended = max(a, b);                     // Lighten
            else blended = b;                                            // Crossfade
            fragColor = vec4(mix(a, blended, clamp(uMix, 0.0, 1.0)), 1.0);
        }
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
        uniform float uTrailDirection; // degrees; 0 = screen up
        uniform float uFlame;
        uniform float uShine;
        uniform float uGummy;
        uniform float uJpegify;
        uniform float uDotMatrix;
        uniform float uColorOverride;
        uniform vec3 uPrimaryColor;
        uniform vec3 uSecondaryColor;

        float hash(vec2 p)
        {
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
        }

        // Bright-pass tap for the Shine streaks: only genuinely hot pixels
        // contribute, so streaks grow out of highlights, not the whole frame.
        vec3 brightTap(sampler2D tex, vec2 p)
        {
            vec3 s = texture(tex, clamp(p, 0.0, 1.0)).rgb;
            float lum = dot(s, vec3(0.299, 0.587, 0.114));
            return s * smoothstep(0.55, 0.95, lum);
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

            // Gummy: soft jelly refraction -- a slow, audio-breathing
            // screen-space wobble applied to the sampling coordinate itself,
            // so the whole picture squishes like lit gelatin.
            if (uGummy > 0.001)
            {
                float wob = uGummy * (0.006 + 0.004 * uLevel + 0.003 * uOnset);
                uv += vec2(sin(uv.y * 21.0 + uTime * 1.7), cos(uv.x * 19.0 - uTime * 1.3)) * wob;
                uv = clamp(uv, 0.0, 1.0);
            }

            vec2 rawUv = uv;
            vec2 historyUv = uv;
            float rgbSplit = 0.0;

            // Directional flame trails: sample last frame's output displaced
            // opposite the travel direction (0 deg = up), with sideways
            // turbulence, so bright content streams/licks along it frame
            // over frame like rising fire. The blend happens below, after
            // history is sampled.
            vec2 flameDir = vec2(sin(radians(uTrailDirection)), cos(radians(uTrailDirection)));
            if (uFlame > 0.001)
            {
                vec2 flamePerp = vec2(flameDir.y, -flameDir.x);
                float turb = (hash(vec2(dot(uv, flamePerp) * 43.0, floor(uTime * 24.0))) - 0.5) * 0.008
                           + sin(dot(uv, flamePerp) * 37.0 + uTime * 5.0) * 0.002;
                historyUv -= flameDir * (0.002 + 0.005 * uFlame + 0.004 * uOnset);
                historyUv += flamePerp * turb * uFlame;
            }

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

                historyUv = clamp(historyUv + offset, 0.0, 1.0); // additive, so Flame's displacement survives

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

            // Jpegify: cheap-but-convincing fake JPEG artifacts -- not a
            // literal per-block DCT (way too expensive per-pixel for no
            // visual gain here), just the three things an eye actually
            // reads as "that's a jpeg": blocky quantization, chroma
            // subsampling (color resampled at a coarser grid than
            // brightness, so saturated color bleeds past sharp edges --
            // the real 4:2:0 artifact), and a little ringing right at
            // block boundaries (mosquito noise's actual signature).
            if (uJpegify > 0.001)
            {
                float blockPx = mix(4.0, 20.0, clamp(uJpegify, 0.0, 1.0));
                vec2 texel = 1.0 / max(uResolution, vec2(1.0));
                vec2 blockSize = texel * blockPx;
                vec2 blockUv = (floor(rawUv / blockSize) + 0.5) * blockSize;
                vec2 chromaBlockUv = (floor(rawUv / (blockSize * 2.0)) + 0.5) * (blockSize * 2.0);

                vec3 blockCol = texture(uRaw, clamp(blockUv, 0.0, 1.0)).rgb;
                vec3 chromaCol = texture(uRaw, clamp(chromaBlockUv, 0.0, 1.0)).rgb;

                // Keep the blocky luma, borrow color from the coarser
                // chroma block -- sharp-brightness-with-bled-color is the
                // single biggest "that's compressed" tell.
                float blockLum = dot(blockCol, vec3(0.299, 0.587, 0.114));
                float chromaLum = dot(chromaCol, vec3(0.299, 0.587, 0.114));
                vec3 jpeg = chromaCol + (blockLum - chromaLum);

                vec2 neighborUv = clamp(blockUv + sign(rawUv - blockUv) * blockSize, 0.0, 1.0);
                vec3 neighborCol = texture(uRaw, neighborUv).rgb;
                float edgeStrength = length(blockCol - neighborCol);
                vec2 withinBlock = (rawUv - (blockUv - blockSize * 0.5)) / blockSize;
                float ring = sin(withinBlock.x * 25.13) * sin(withinBlock.y * 25.13) * edgeStrength * 0.5;
                jpeg += ring;

                raw = mix(raw, jpeg, clamp(uJpegify, 0.0, 1.0));
            }
    )GLSL" R"GLSL(
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

            // Shine: anisotropic star-streak specular -- bright-pass taps
            // marched along two crossed diagonals, weighted down with
            // distance, so hot spots grow gleaming star arms. Onsets flare
            // the streaks.
            if (uShine > 0.001)
            {
                vec2 texel = 1.0 / max(uResolution, vec2(1.0));
                vec2 arm1 = vec2(1.0, 0.35) * texel * 4.0;
                vec2 arm2 = vec2(-0.35, 1.0) * texel * 4.0;
                vec3 streak = vec3(0.0);
                for (int s = 1; s <= 5; ++s)
                {
                    float wgt = 1.0 / (float(s) * 1.5);
                    streak += (brightTap(uRaw, rawUv + arm1 * float(s)) + brightTap(uRaw, rawUv - arm1 * float(s))
                             + brightTap(uRaw, rawUv + arm2 * float(s)) + brightTap(uRaw, rawUv - arm2 * float(s))) * wgt;
                }
                raw += streak * uShine * (0.35 + uOnset * 0.5);
                // Glossy pop: gentle s-curve so highlights read wet/polished.
                raw = mix(raw, smoothstep(vec3(0.0), vec3(1.0), raw) * 1.05, uShine * 0.4);
            }

            vec3 history = texture(uHistory, clamp(historyUv, vec2(0.002), vec2(0.998))).rgb;

            // Trails: exponential moving average against last frame's fully
            // processed output -- a bounded, standard phosphor-persistence
            // blend, so it can't blow out or decay to black on its own.
            vec3 col = mix(raw, history, clamp(uTrails, 0.0, 0.97));

            // Flame blend: bright history content decays warm (embers cool
            // toward red) while streaming along the trail direction; max()
            // keeps flames licking over dark areas without dimming the
            // live image underneath.
            if (uFlame > 0.001)
            {
                vec3 ember = history * vec3(0.97, 0.86, 0.70);
                col = max(col, ember * (0.5 + 0.46 * uFlame));
            }

            // Gummy's milky lift: soften the response curve so everything
            // reads translucent and soft-bodied.
            if (uGummy > 0.001)
                col = mix(col, pow(max(col, 0.0), vec3(0.78)) * 0.92 + 0.015, uGummy * 0.4);

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

            // Dot Matrix: an audio-reactive halftone/particle-grid overlay
            // -- a regular grid of dots sized by the underlying picture's
            // local brightness (so it reads as "the image, but rendered
            // as an LED/particle grid") and per-cell twinkling so it
            // feels alive rather than a static screen filter. Bass/onset
            // pulse the whole grid's scale -- the "particle" half of the
            // ask, without needing an actual simulated particle buffer.
            if (uDotMatrix > 0.001)
            {
                float cellPx = mix(26.0, 9.0, clamp(uDotMatrix, 0.0, 1.0));
                vec2 cellUv = uv * uResolution / cellPx;
                vec2 cellId = floor(cellUv);
                vec2 cellFrac = fract(cellUv) - 0.5;

                vec2 sampleUv = clamp((cellId + 0.5) * cellPx / uResolution, 0.0, 1.0);
                vec3 cellCol = texture(uRaw, sampleUv).rgb;
                float lum = dot(cellCol, vec3(0.299, 0.587, 0.114));

                float twinkle = 0.55 + 0.45 * sin(uTime * (1.5 + hash(cellId) * 2.2) + hash(cellId) * 40.0);
                float pulse = 1.0 + uLevel * 0.6 + uOnset * 1.1;
                float radius = clamp(lum * 0.48 * pulse * twinkle, 0.0, 0.48);

                float dotMask = 1.0 - smoothstep(radius - 0.08, radius, length(cellFrac));
                vec3 dotted = cellCol * (1.3 + uOnset * 0.7) * dotMask;

                col = mix(col, dotted, uDotMatrix);
            }

            // Duotone color override: remap the whole image onto a
            // luminance gradient between two user-picked colors, the same
            // technique as a classic photo duotone effect. Default off
            // (uColorOverride = 0) so the picked colors never matter unless
            // this is actually turned up.
            if (uColorOverride > 0.001)
            {
                float lum = dot(clamp(col, 0.0, 1.0), vec3(0.299, 0.587, 0.114));
                vec3 duotone = mix(uSecondaryColor, uPrimaryColor, lum);
                col = mix(col, duotone, clamp(uColorOverride, 0.0, 1.0));
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
    palette                 = makeUniformIfPresent(program, "uPalette");
    prevFrame               = makeUniformIfPresent(program, "uPrevFrame");
    waveform                = makeUniformIfPresent(program, "uWaveform");
    fractalOrbit            = makeUniformIfPresent(program, "uFractalOrbit");
    fractalOrbitLength      = makeUniformIfPresent(program, "uFractalOrbitLength");
    fractalRadius           = makeUniformIfPresent(program, "uFractalRadius");
    fractalFade             = makeUniformIfPresent(program, "uFractalFade");
    fractalRefOffset        = makeUniformIfPresent(program, "uFractalRefOffset");
    fractalIterNeed         = makeUniformIfPresent(program, "uFractalIterNeed");
    ifsZoomScale            = makeUniformIfPresent(program, "uIfsZoomScale");
    ifsFade                 = makeUniformIfPresent(program, "uIfsFade");
    zoomPhase               = makeUniformIfPresent(program, "uZoomPhase");
    userImage               = makeUniformIfPresent(program, "uUserImage");
    userImageAspect         = makeUniformIfPresent(program, "uUserImageAspect");
    userImageLoaded         = makeUniformIfPresent(program, "uUserImageLoaded");
    pipeJoints              = makeUniformIfPresent(program, "uPipeJoints");
    pipeHuesA               = makeUniformIfPresent(program, "uPipeHuesA");
    pipeHueE                = makeUniformIfPresent(program, "uPipeHueE");
    mazePos                 = makeUniformIfPresent(program, "uMazePos");
    mazeHeading             = makeUniformIfPresent(program, "uMazeHeading");
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
    blendMode = makeUniformIfPresent(program, "uBlendMode");
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
    trailDirection = makeUniformIfPresent(program, "uTrailDirection");
    flame = makeUniformIfPresent(program, "uFlame");
    shine = makeUniformIfPresent(program, "uShine");
    gummy = makeUniformIfPresent(program, "uGummy");
    jpegify = makeUniformIfPresent(program, "uJpegify");
    dotMatrix = makeUniformIfPresent(program, "uDotMatrix");
    colorOverride = makeUniformIfPresent(program, "uColorOverride");
    primaryColor = makeUniformIfPresent(program, "uPrimaryColor");
    secondaryColor = makeUniformIfPresent(program, "uSecondaryColor");
}

VisualizerRenderer::VisualizerRenderer(AudioAnalyzer& analyzerToUse, VisualizerParameterRefs& paramsToUse)
    : analyzer(analyzerToUse), params(paramsToUse)
{
    // Preset indices must match PresetNames::all in VisualizerParameters.h.
    // Two autopilot dive presets (boundary-bisection locked -- see
    // FractalNavigator.h) and five manually-navigated explorers. The
    // ship-family explorers pass panYSign -1 because their shaders flip the
    // imaginary axis into the classic orientation.
    using Mode = FractalNavigator::Mode;
    fractalSlots.reserve(7);
    fractalSlots.emplace_back(0, FractalFormula::Mandelbrot, Mode::Autopilot, -0.745, 0.11, 1.4);
    fractalSlots.emplace_back(5, FractalFormula::BurningShip, Mode::Autopilot, -1.75, -0.03, 1.4);
    fractalSlots.emplace_back(15, FractalFormula::Mandelbrot, Mode::Manual, -0.6, 0.0, 2.6);
    fractalSlots.emplace_back(16, FractalFormula::BurningShip, Mode::Manual, -0.45, -0.5, 2.4, -1.0);
    fractalSlots.emplace_back(17, FractalFormula::PerpendicularShip, Mode::Manual, -0.45, -0.4, 2.4, -1.0);
    fractalSlots.emplace_back(18, FractalFormula::Buffalo, Mode::Manual, -0.6, -0.35, 2.6, -1.0);
    fractalSlots.emplace_back(19, FractalFormula::Tricorn, Mode::Manual, -0.25, 0.0, 2.8);
}

void VisualizerRenderer::requestManualZoom(float steps)
{
    pendingZoomSteps.fetch_add(steps);
}

void VisualizerRenderer::requestManualPan(float dxFraction, float dyFraction)
{
    pendingPanX.fetch_add(dxFraction);
    pendingPanY.fetch_add(dyFraction);
}

VisualizerRenderer::FractalSlot* VisualizerRenderer::slotForPreset(int presetIndex)
{
    for (auto& slot : fractalSlots)
        if (slot.presetIndex == presetIndex)
            return &slot;
    return nullptr;
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
    // Multisampling off: a multisampled pixel format request can fail (or
    // silently downgrade to something the GL context can't actually use)
    // when the window being attached to is itself embedded in a host's
    // own compositor rather than a plain top-level window -- reported
    // symptom is exactly this: the JUCE Component chrome (Preset combo,
    // sliders, buttons) paints fine since that's normal software
    // compositing, but the OpenGL-rendered visual area is solid black,
    // with nothing at all reaching the shader-compile-error log, meaning
    // the context likely never finishes attaching in the first place.
    // Standalone was never affected (its window is always a plain
    // top-level one), which is why this went uncaught until it was
    // actually tested inside a DAW.
    context.setMultisamplingEnabled(false);
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
    // Always logged, not just on failure: this is the one line that
    // proves the GL context actually attached at all inside whatever
    // window it's embedded in -- its total absence from the log is what
    // diagnosed the black-in-a-DAW-host report that led to the
    // setMultisamplingEnabled(false) change above (attachTo()'s comment
    // has the full story). If a render is ever reported black again with
    // this line present in the log, the context DID attach and the bug
    // is somewhere else (shader compile, framebuffer sizing, etc.).
    juce::Logger::writeToLog("VisualizerRenderer: GL context created, renderer = "
                              + juce::String((const char*) glGetString(GL_RENDERER))
                              + ", version = " + juce::String((const char*) glGetString(GL_VERSION)));

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

    // Perturbation reference orbit textures, one per fractal slot: one
    // texel per iteration, RGBA32F = (re.hi, re.lo, im.hi, im.lo). Nearest
    // filtering + texelFetch on the GPU side, since these are indexed
    // exactly by integer iteration, not sampled/interpolated. Marking every
    // orbit dirty forces a re-upload into the fresh GL context.
    for (auto& slot : fractalSlots)
    {
        glGenTextures(1, &slot.texture);
        glBindTexture(GL_TEXTURE_2D, slot.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FractalNavigator::maxReferenceIterations, 1, 0, GL_RGBA,
                     GL_FLOAT, nullptr);
        if (slot.nav.referenceOrbitLength() > 0)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, slot.nav.referenceOrbitLength(), 1, GL_RGBA, GL_FLOAT,
                            slot.nav.referenceOrbitData().data());
    }

    glGenTextures(1, &userImageTexture);
    glBindTexture(GL_TEXTURE_2D, userImageTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Pipes joint-position texture: one texel per joint, RGBA32F =
    // (x, y, z, radius). Nearest + texelFetch, same reasoning as the
    // fractal reference-orbit textures -- indexed exactly by integer
    // joint, never sampled/interpolated.
    glGenTextures(1, &pipeJointTexture);
    glBindTexture(GL_TEXTURE_2D, pipeJointTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, PipeNetwork::textureWidth, 1, 0, GL_RGBA, GL_FLOAT,
                 pipeNetwork.jointData().data());
    // A fresh GL context has no texture data regardless of what was
    // uploaded before (e.g. the host tore down and recreated the context);
    // if a picture was already loaded, force the next frame to re-upload
    // it rather than silently going blank.
    {
        std::lock_guard<std::mutex> lock(userImageMutex);
        if (userImageLoaded && ! pendingFrames.empty())
            userImageDirty = true;
    }

    lastWidth = lastHeight = 0;
    startTimeMs = juce::Time::getMillisecondCounterHiRes();
    lastFrameTimeMs = startTimeMs;
    onsetEnvelope = 0.0f;
    ifsZoomScale = 1.0;
    ifsTimeSinceReset = 0.0;
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

    if (userImageTexture != 0)
    {
        glDeleteTextures(1, &userImageTexture);
        userImageTexture = 0;
    }

    if (pipeJointTexture != 0)
    {
        glDeleteTextures(1, &pipeJointTexture);
        pipeJointTexture = 0;
    }

    for (auto& slot : fractalSlots)
    {
        if (slot.texture != 0)
        {
            glDeleteTextures(1, &slot.texture);
            slot.texture = 0;
        }
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

    // Manual input from the editor goes to the active preset's slot only
    // (and is consumed regardless, so stale input can't pile up and replay
    // when switching onto an explorer preset).
    const float zoomSteps = pendingZoomSteps.exchange(0.0f);
    const float panDx = pendingPanX.exchange(0.0f);
    const float panDy = pendingPanY.exchange(0.0f);
    const int activePreset = params.presetIndex != nullptr ? juce::roundToInt(params.presetIndex->load()) : 0;

    for (auto& slot : fractalSlots)
    {
        if (slot.manual)
        {
            if (slot.presetIndex == activePreset)
            {
                if (zoomSteps != 0.0f)
                    slot.nav.zoomBy(std::pow(0.9, (double) zoomSteps));
                if (panDx != 0.0f || panDy != 0.0f)
                    slot.nav.panBy((double) -panDx, (double) panDy * slot.panYSign);
            }
            slot.nav.tick((double) dt);
        }
        else
        {
            slot.nav.update((double) dt, rate, navigatorRng);
        }
        uploadOrbitTextureIfDirty(slot.nav, slot.texture);
    }

    // Pipes' growth speed: Zoom Speed and Camera Shake repurposed as
    // "how fast things build" (Pipes has no zoom of its own), same
    // audio-reactive shape as `rate` above but expressed directly as grid
    // steps/second rather than a per-second shrink multiplier.
    const double pipeGrowthRate = 3.2 + 5.5 * t
                                 + (double) (analyzer.getBassAutoGain() * 2.0f + onsetEnvelope * 2.6f)
                                       * (double) cameraShakeValue;
    pipeNetwork.update((double) dt, pipeGrowthRate, navigatorRng);
    uploadPipeTextureIfDirty();

    // Maze walk speed/turn-bias: same repurposing idea as Pipes' growth
    // rate above -- Zoom Speed becomes "how fast the walk moves", Camera
    // Shake plus the audio envelope becomes "how eager it is to turn at
    // a junction instead of continuing straight".
    // mazeWalker moves in its own logical units (1 per hop); the shader
    // renders those at a much wider physical pitch (see kPitch in
    // infinite_maze.frag) for roomier corridors, so this constant is
    // tuned down from what it'd be if it were directly a world-space
    // speed, to land at a similar real pace despite that.
    const double mazeSpeed = 0.8 + 1.4 * t + (double) (analyzer.getBassAutoGain() * 0.45f) * (double) cameraShakeValue;
    const double mazeTurnBias = 0.3 + (double) (onsetEnvelope * 0.5f) * (double) cameraShakeValue;
    mazeWalker.update((double) dt, mazeSpeed, mazeTurnBias, navigatorRng);

    // Sierpinski's seamless-wrap zoom phase: the fractional part of the
    // accumulated log2 zoom depth, advancing at the same audio-reactive
    // rate as every other dive so it stays musically in sync. Wrapping the
    // exponent (instead of ever holding a huge zoom value) is what makes
    // that dive literally infinite in plain float precision.
    zoomPhase += (double) dt * -std::log2(rate);
    zoomPhase -= std::floor(zoomPhase);

    // Unbounded zoom scale for the exactly-self-similar IFS/DE presets
    // (Sierpinski, Apollonian, Julia): multiplied down every frame just
    // like FractalNavigator's viewRadius (same `rate`, so all the dive-
    // style presets stay in sync with Zoom Speed/Camera Shake), NOT
    // recomputed from a growing log-depth via exp() -- that would
    // underflow float precision long before the scale itself needs to get
    // that small. Reset far short of double-float's ~1e13 wall (with a
    // safety margin, same reasoning as FractalNavigator's radiusFloor) so
    // the loop-back happens while detail is still crisp.
    ifsZoomScale *= std::pow(rate, (double) dt);
    ifsTimeSinceReset += (double) dt;
    constexpr double ifsZoomFloor = 1.0e-11;
    if (ifsZoomScale < ifsZoomFloor)
    {
        ifsZoomScale = 1.0;
        ifsTimeSinceReset = 0.0;
    }
}

void VisualizerRenderer::uploadOrbitTextureIfDirty(FractalNavigator& nav, GLuint texture)
{
    if (! nav.consumeOrbitDirty())
        return;
    const int len = nav.referenceOrbitLength();
    if (len <= 0)
        return;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, len, 1, GL_RGBA, GL_FLOAT, nav.referenceOrbitData().data());
}

void VisualizerRenderer::updateWaveformTexture()
{
    analyzer.copyWaveform(waveformSnapshot);
    glBindTexture(GL_TEXTURE_2D, waveformTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, AudioAnalyzer::waveformSize, 1, GL_RED, GL_FLOAT,
                     waveformSnapshot.data());
}

void VisualizerRenderer::uploadPipeTextureIfDirty()
{
    if (! pipeNetwork.consumeDirty())
        return;
    glBindTexture(GL_TEXTURE_2D, pipeJointTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, PipeNetwork::textureWidth, 1, GL_RGBA, GL_FLOAT,
                    pipeNetwork.jointData().data());
}

void VisualizerRenderer::setSourceImage(const juce::Image& image)
{
    // Runs on the message thread (the editor's FileChooser callback, or
    // the constructor reloading a previously-saved path) -- all the CPU
    // decode work happens here, deliberately, so the GL thread only ever
    // has to do the actual upload.
    if (! image.isValid())
        return;

    const auto argb = image.convertedToFormat(juce::Image::ARGB);
    const int w = argb.getWidth();
    const int h = argb.getHeight();
    if (w <= 0 || h <= 0)
        return;

    std::vector<juce::uint8> pixels((size_t) w * (size_t) h * 4);
    {
        const juce::Image::BitmapData bd(argb, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
        {
            const auto* row = reinterpret_cast<const juce::PixelARGB*>(bd.getLinePointer(y));
            juce::uint8* out = pixels.data() + (size_t) y * (size_t) w * 4;
            for (int x = 0; x < w; ++x)
            {
                const auto& px = row[x];
                out[x * 4 + 0] = px.getRed();
                out[x * 4 + 1] = px.getGreen();
                out[x * 4 + 2] = px.getBlue();
                out[x * 4 + 3] = px.getAlpha();
            }
        }
    }

    std::vector<std::vector<juce::uint8>> frames;
    frames.push_back(std::move(pixels));

    std::lock_guard<std::mutex> lock(userImageMutex);
    pendingFrames = std::move(frames);
    pendingFrameDelaysMs = { 0 }; // single frame -- 0 means "never advance", see uploadUserImageIfDirty
    pendingImageWidth = w;
    pendingImageHeight = h;
    userImageDirty = true;
    userImageLoaded = true;
    userImageAspect = (float) w / (float) juce::jmax(h, 1);
}

void VisualizerRenderer::setSourceImageAnimated(std::vector<std::vector<juce::uint8>> frames,
                                                 std::vector<int> frameDelaysMs, int width, int height)
{
    if (frames.empty() || width <= 0 || height <= 0)
        return;
    // Defensive: mismatched sizes would desync frames/delays indexing in
    // uploadUserImageIfDirty. Pad rather than reject -- a mis-decoded
    // delay list is a cosmetic timing bug, not worth throwing the whole
    // load away for.
    frameDelaysMs.resize(frames.size(), 100);

    std::lock_guard<std::mutex> lock(userImageMutex);
    pendingFrames = std::move(frames);
    pendingFrameDelaysMs = std::move(frameDelaysMs);
    pendingImageWidth = width;
    pendingImageHeight = height;
    userImageDirty = true;
    userImageLoaded = true;
    userImageAspect = (float) width / (float) juce::jmax(height, 1);
}

void VisualizerRenderer::uploadUserImageIfDirty(float dtSeconds)
{
    // GL thread. userImageTexture is only 0 before the first
    // newOpenGLContextCreated() -- guard rather than upload into "no
    // texture bound".
    if (userImageTexture == 0)
        return;

    bool needsUpload = false;
    {
        std::lock_guard<std::mutex> lock(userImageMutex);
        if (userImageDirty)
        {
            // Small relative to a one-off image load / a GIF's total frame
            // count; keeps the mutex held time short either way. Width/
            // height are copied under the same lock as the frame data --
            // otherwise a load arriving between this copy and the
            // glTexImage2D call below could pair one image's pixels with
            // another's dimensions.
            activeFrames = pendingFrames;
            activeFrameDelaysMs = pendingFrameDelaysMs;
            activeImageWidth = pendingImageWidth;
            activeImageHeight = pendingImageHeight;
            userImageDirty = false;
            activeFrameIndex = 0;
            frameElapsedMs = 0.0;
            needsUpload = true;
        }
    }

    if (activeFrames.empty())
        return;

    if (! needsUpload && activeFrames.size() > 1)
    {
        // Advance through however many frames this dt actually covers --
        // a stalled/backgrounded window catching back up shouldn't just
        // show one frame for a giant dt and silently drop the rest.
        frameElapsedMs += (double) dtSeconds * 1000.0;
        int guard = 0;
        while (activeFrameIndex < activeFrameDelaysMs.size()
               && frameElapsedMs >= (double) activeFrameDelaysMs[activeFrameIndex] && ++guard < 64)
        {
            frameElapsedMs -= (double) activeFrameDelaysMs[activeFrameIndex];
            activeFrameIndex = (activeFrameIndex + 1) % activeFrames.size();
            needsUpload = true;
        }
    }

    if (! needsUpload)
        return;

    activeFrameIndex = juce::jmin(activeFrameIndex, activeFrames.size() - 1);
    const auto& pixels = activeFrames[activeFrameIndex];
    if (pixels.empty() || activeImageWidth <= 0 || activeImageHeight <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, userImageTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, activeImageWidth, activeImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
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
    setU(u.palette, params.palette != nullptr ? params.palette->load() : 0.0f);

    float ifsZoomHi = 0.0f, ifsZoomLo = 0.0f;
    ddToFloatPair(ddFromDouble(ifsZoomScale), ifsZoomHi, ifsZoomLo);
    setU(u.ifsZoomScale, ifsZoomHi, ifsZoomLo);
    const double ifsFadeT = juce::jlimit(0.0, 1.0, ifsTimeSinceReset / 1.5);
    setU(u.ifsFade, (float) (ifsFadeT * ifsFadeT * (3.0 - 2.0 * ifsFadeT)));

    setU(u.zoomPhase, (float) zoomPhase);

    // Fractal slot uniforms: the active preset's own navigator if it has
    // one, otherwise the first slot (harmless -- presets without a slot
    // never read these uniforms).
    FractalSlot* slot = slotForPreset(presetIndex);
    if (slot == nullptr && ! fractalSlots.empty())
        slot = &fractalSlots.front();

    GLuint orbitTex = 0;
    if (slot != nullptr)
    {
        const FractalNavigator& nav = slot->nav;
        orbitTex = slot->texture;
        // The manual explorers control zoom themselves -- Camera Scale
        // stays an autopilot-preset convenience so it can't silently fight
        // the wheel/arrow zoom.
        const float radiusScale = slot->manual ? 1.0f : cameraScaleValue;
        setU(u.fractalRadius, (float) nav.radius() * radiusScale);
        setU(u.fractalFade, nav.fadeEnvelope());
        setU(u.fractalOrbitLength, (float) nav.referenceOrbitLength());
        float xHi = 0.0f, xLo = 0.0f, yHi = 0.0f, yLo = 0.0f;
        nav.getReferenceOffset(xHi, xLo, yHi, yLo);
        setU(u.fractalRefOffset, xHi, xLo, yHi, yLo);
        setU(u.fractalIterNeed, nav.recommendedIterCap());
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prevFrameTex);
    setU(u.prevFrame, (GLint) 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, waveformTexture);
    setU(u.waveform, (GLint) 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, orbitTex);
    setU(u.fractalOrbit, (GLint) 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, userImageTexture);
    setU(u.userImage, (GLint) 3);
    setU(u.userImageAspect, userImageAspect);
    setU(u.userImageLoaded, userImageLoaded ? 1.0f : 0.0f);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, pipeJointTexture);
    setU(u.pipeJoints, (GLint) 4);
    const auto& hues = pipeNetwork.hues();
    setU(u.pipeHuesA, hues[0], hues[1], hues[2], hues[3]);
    setU(u.pipeHueE, hues[4]);

    setU(u.mazePos, mazeWalker.posX(), mazeWalker.posZ());
    setU(u.mazeHeading, mazeWalker.headingX(), mazeWalker.headingZ());
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
    uploadUserImageIfDirty(dt);
    updateNavigators(dt);

    const int numPresets = (int) presets.size();
    if (numPresets == 0)
        return;

    const float presetRaw = params.presetIndex != nullptr ? params.presetIndex->load() : 0.0f;
    const int presetA = juce::jlimit(0, numPresets - 1, juce::roundToInt(presetRaw));
    // Layer B used to be forced to "whatever's next in the list" -- now an
    // independent user choice (see ParamIDs::layerBIndex's comment).
    const float layerBRaw = params.layerBIndex != nullptr ? params.layerBIndex->load() : (float) ((presetA + 1) % numPresets);
    const int presetB = juce::jlimit(0, numPresets - 1, juce::roundToInt(layerBRaw));
    const float morph = params.layerMix != nullptr ? juce::jlimit(0.0f, 1.0f, params.layerMix->load()) : 0.0f;
    const float blendModeRaw = params.blendMode != nullptr ? params.blendMode->load() : 0.0f;

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
            setU(blendUniforms->blendMode, blendModeRaw);
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
        setU(postUniforms->trailDirection, params.trailDirection != nullptr ? params.trailDirection->load() : 0.0f);
        setU(postUniforms->flame, params.flame != nullptr ? params.flame->load() : 0.0f);
        setU(postUniforms->shine, params.shine != nullptr ? params.shine->load() : 0.0f);
        setU(postUniforms->gummy, params.gummy != nullptr ? params.gummy->load() : 0.0f);
        setU(postUniforms->jpegify, params.jpegify != nullptr ? params.jpegify->load() : 0.0f);
        setU(postUniforms->dotMatrix, params.dotMatrix != nullptr ? params.dotMatrix->load() : 0.0f);
        setU(postUniforms->colorOverride, params.colorOverride != nullptr ? params.colorOverride->load() : 0.0f);
        setU(postUniforms->primaryColor, params.primaryColorR != nullptr ? params.primaryColorR->load() : 1.0f,
             params.primaryColorG != nullptr ? params.primaryColorG->load() : 0.15f,
             params.primaryColorB != nullptr ? params.primaryColorB->load() : 0.65f);
        setU(postUniforms->secondaryColor, params.secondaryColorR != nullptr ? params.secondaryColorR->load() : 0.05f,
             params.secondaryColorG != nullptr ? params.secondaryColorG->load() : 0.25f,
             params.secondaryColorB != nullptr ? params.secondaryColorB->load() : 0.95f);

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
