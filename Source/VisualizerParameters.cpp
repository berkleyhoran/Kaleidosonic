#include "VisualizerParameters.h"
#include <algorithm>

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

namespace
{
    std::unique_ptr<juce::AudioParameterFloat> makeFloat(const juce::String& id, const juce::String& name,
                                                           float lo, float hi, float def,
                                                           float step = 0.0f, float skew = 1.0f)
    {
        juce::NormalisableRange<float> range(lo, hi, step == 0.0f ? (hi - lo) / 1000.0f : step);
        range.setSkewForCentre(juce::jlimit(lo + 1.0e-4f, hi - 1.0e-4f, lo + (hi - lo) * 0.5f * skew));
        return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(id, 1), name, range, def);
    }
}

Layout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::presetIndex, 1), "Preset", PresetNames::all, 0));

    params.push_back(makeFloat(ParamIDs::presetMorph, "Preset Morph", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::reactivity, "Reactivity", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::bassGain, "Bass Gain", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::midGain, "Mid Gain", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::trebleGain, "Treble Gain", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::zoomSpeed, "Zoom Speed", -1.0f, 1.0f, 0.3f));
    params.push_back(makeFloat(ParamIDs::rotationSpeed, "Rotation Speed", -1.0f, 1.0f, 0.2f));
    params.push_back(makeFloat(ParamIDs::hue, "Hue", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::saturation, "Saturation", 0.0f, 1.0f, 0.8f));
    params.push_back(makeFloat(ParamIDs::brightness, "Brightness", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::contrast, "Contrast", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::kaleidoscopeSegments, "Kaleidoscope Segments", 1.0f, 16.0f, 6.0f));
    params.push_back(makeFloat(ParamIDs::feedbackAmount, "Feedback Amount", 0.0f, 0.98f, 0.9f));
    params.push_back(makeFloat(ParamIDs::iterations, "Iterations", 4.0f, 64.0f, 24.0f));
    params.push_back(makeFloat(ParamIDs::distortion, "Distortion", 0.0f, 1.0f, 0.3f));
    params.push_back(makeFloat(ParamIDs::zoomWander, "Zoom Wander", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::cameraShake, "Camera Shake", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::cameraScale, "Camera Scale", 0.2f, 6.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::palette, "Palette", 0.0f, 8.0f, 0.0f));

    params.push_back(makeFloat(ParamIDs::trails, "Trails", 0.0f, 0.97f, 0.0f));
    params.push_back(makeFloat(ParamIDs::blur, "Blur", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::noiseAmount, "Noise", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::datamosh, "Datamosh", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::bloomIntensity, "Bloom Intensity", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::vignette, "Vignette", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::chromaticAberration, "Chromatic Aberration", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::colorCycleSpeed, "Color Cycle Speed", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::pulseDepth, "Pulse Depth", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::posterize, "Posterize", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::fisheye, "Fisheye", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::trailDirection, "Trail Direction", -180.0f, 180.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::flame, "Flame", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::shine, "Shine", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::gummy, "Gummy", 0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}

const std::vector<ParamGroupInfo>& paramGroups()
{
    static const std::vector<ParamGroupInfo> groups {
        { "Audio Reactivity", { ParamIDs::reactivity, ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain } },
        { "Motion & Zoom", { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::cameraShake,
                              ParamIDs::cameraScale, ParamIDs::zoomWander } },
        { "Fractal Detail", { ParamIDs::iterations, ParamIDs::kaleidoscopeSegments, ParamIDs::distortion,
                               ParamIDs::feedbackAmount } },
        { "Color", { ParamIDs::hue, ParamIDs::saturation, ParamIDs::brightness, ParamIDs::contrast,
                      ParamIDs::palette } },
        { "Post FX: Trails & Flame", { ParamIDs::trails, ParamIDs::trailDirection, ParamIDs::flame } },
        { "Post FX: Filters", { ParamIDs::blur, ParamIDs::noiseAmount, ParamIDs::datamosh, ParamIDs::shine,
                                 ParamIDs::gummy, ParamIDs::posterize, ParamIDs::fisheye,
                                 ParamIDs::chromaticAberration } },
        { "Post FX: Glow & Cycle", { ParamIDs::bloomIntensity, ParamIDs::vignette, ParamIDs::colorCycleSpeed,
                                      ParamIDs::pulseDepth } },
    };
    return groups;
}

namespace
{
    // Grounded in the actual shaders: which conditionally-relevant params
    // (i.e. not one of the always-relevant ones below) each preset's .frag
    // file -- and the common.glsl helpers it calls (fractalLayer/
    // exploreFractal/perturbEscapeTime for the escape-time presets) --
    // actually reads, plus the handful (Zoom Speed for the autopilot/
    // IFS-style presets) that are only ever consumed in C++ before
    // reaching a shader at all. Index matches PresetNames::all. Rebuilt by
    // hand from `grep` over Shaders/*.frag any time a preset's uniform
    // usage changes -- see the comment on isParamRelevantForPreset.
    const std::vector<std::vector<juce::String>>& conditionalRelevance()
    {
        static const std::vector<std::vector<juce::String>> table {
            /* 0  Mandelbrot Pulse */
            { ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments,
              ParamIDs::iterations, ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::cameraScale,
              ParamIDs::palette },
            /* 1  Julia Kaleidoscope */
            { ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::iterations, ParamIDs::distortion, ParamIDs::zoomWander,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 2  Plasma Feedback */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed,
              ParamIDs::rotationSpeed, ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::feedbackAmount },
            /* 3  IFS Tunnel */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed,
              ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::cameraScale },
            /* 4  Tunnel Spiral */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::iterations, ParamIDs::distortion, ParamIDs::cameraShake,
              ParamIDs::cameraScale },
            /* 5  Burning Ship */
            { ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments,
              ParamIDs::iterations, ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::cameraScale,
              ParamIDs::palette },
            /* 6  Apollonian Gasket */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed,
              ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments, ParamIDs::distortion, ParamIDs::zoomWander,
              ParamIDs::cameraShake, ParamIDs::cameraScale },
            /* 7  Raymarch Tunnel 3D */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed,
              ParamIDs::rotationSpeed, ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::cameraScale },
            /* 8  Particle Bloom */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::cameraScale },
            /* 9  Oscilloscope Glow */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::distortion },
            /* 10 Waveform Scope */
            { ParamIDs::trebleGain },
            /* 11 Sierpinski Triforce */
            { ParamIDs::bassGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 12 Fractal Bubbles */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed,
              ParamIDs::zoomWander, ParamIDs::cameraShake, ParamIDs::cameraScale },
            /* 13 Starfield Warp */
            { ParamIDs::bassGain, ParamIDs::zoomSpeed, ParamIDs::zoomWander, ParamIDs::cameraShake,
              ParamIDs::cameraScale },
            /* 14 Mandelbox */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 15 Mandelbrot Explorer */
            { ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::palette },
            /* 16 Burning Ship Explorer */
            { ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::palette },
            /* 17 Perpendicular Ship */
            { ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::palette },
            /* 18 Buffalo Fractal */
            { ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::palette },
            /* 19 Tricorn */
            { ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::palette },
            /* 20 Burning Ship 3D */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::rotationSpeed, ParamIDs::iterations,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 21 Audio Nebula */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 22 Image Ripple */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::distortion,
              ParamIDs::cameraScale, ParamIDs::palette },
            /* 23 Image Shatter */
            { ParamIDs::bassGain, ParamIDs::distortion, ParamIDs::cameraScale, ParamIDs::palette },
            /* 24 Image Kaleidoscope */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments,
              ParamIDs::zoomWander, ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
        };
        return table;
    }
}

bool isParamRelevantForPreset(int presetIndex, const juce::String& paramID)
{
    // Always relevant, for every preset: color grading (every preset's main()
    // ends by calling grade(), directly or via exploreFractal) and every
    // global post-FX parameter (the post pass runs after whichever preset
    // rendered, regardless of what that preset used).
    static const std::vector<juce::String> alwaysRelevant {
        ParamIDs::reactivity, ParamIDs::hue, ParamIDs::saturation, ParamIDs::brightness, ParamIDs::contrast,
        ParamIDs::trails, ParamIDs::blur, ParamIDs::noiseAmount, ParamIDs::datamosh, ParamIDs::bloomIntensity,
        ParamIDs::vignette, ParamIDs::chromaticAberration, ParamIDs::colorCycleSpeed, ParamIDs::pulseDepth,
        ParamIDs::posterize, ParamIDs::fisheye, ParamIDs::trailDirection, ParamIDs::flame, ParamIDs::shine,
        ParamIDs::gummy,
    };
    if (std::find(alwaysRelevant.begin(), alwaysRelevant.end(), paramID) != alwaysRelevant.end())
        return true;

    const auto& table = conditionalRelevance();
    if (presetIndex < 0 || (size_t) presetIndex >= table.size())
        return true; // unknown index -- fail open rather than grey out incorrectly

    const auto& relevant = table[(size_t) presetIndex];
    return std::find(relevant.begin(), relevant.end(), paramID) != relevant.end();
}

void VisualizerParameterRefs::resolve(juce::AudioProcessorValueTreeState& apvts)
{
    presetIndex          = apvts.getRawParameterValue(ParamIDs::presetIndex);
    presetMorph           = apvts.getRawParameterValue(ParamIDs::presetMorph);
    reactivity            = apvts.getRawParameterValue(ParamIDs::reactivity);
    bassGain              = apvts.getRawParameterValue(ParamIDs::bassGain);
    midGain                = apvts.getRawParameterValue(ParamIDs::midGain);
    trebleGain             = apvts.getRawParameterValue(ParamIDs::trebleGain);
    zoomSpeed              = apvts.getRawParameterValue(ParamIDs::zoomSpeed);
    rotationSpeed          = apvts.getRawParameterValue(ParamIDs::rotationSpeed);
    hue                     = apvts.getRawParameterValue(ParamIDs::hue);
    saturation              = apvts.getRawParameterValue(ParamIDs::saturation);
    brightness              = apvts.getRawParameterValue(ParamIDs::brightness);
    contrast                = apvts.getRawParameterValue(ParamIDs::contrast);
    kaleidoscopeSegments    = apvts.getRawParameterValue(ParamIDs::kaleidoscopeSegments);
    feedbackAmount          = apvts.getRawParameterValue(ParamIDs::feedbackAmount);
    iterations              = apvts.getRawParameterValue(ParamIDs::iterations);
    distortion              = apvts.getRawParameterValue(ParamIDs::distortion);
    zoomWander              = apvts.getRawParameterValue(ParamIDs::zoomWander);
    cameraShake             = apvts.getRawParameterValue(ParamIDs::cameraShake);
    cameraScale             = apvts.getRawParameterValue(ParamIDs::cameraScale);
    palette                 = apvts.getRawParameterValue(ParamIDs::palette);

    trails                  = apvts.getRawParameterValue(ParamIDs::trails);
    blur                    = apvts.getRawParameterValue(ParamIDs::blur);
    noiseAmount             = apvts.getRawParameterValue(ParamIDs::noiseAmount);
    datamosh                = apvts.getRawParameterValue(ParamIDs::datamosh);
    bloomIntensity          = apvts.getRawParameterValue(ParamIDs::bloomIntensity);
    vignette                = apvts.getRawParameterValue(ParamIDs::vignette);
    chromaticAberration     = apvts.getRawParameterValue(ParamIDs::chromaticAberration);
    colorCycleSpeed         = apvts.getRawParameterValue(ParamIDs::colorCycleSpeed);
    pulseDepth              = apvts.getRawParameterValue(ParamIDs::pulseDepth);
    posterize               = apvts.getRawParameterValue(ParamIDs::posterize);
    fisheye                 = apvts.getRawParameterValue(ParamIDs::fisheye);
    trailDirection          = apvts.getRawParameterValue(ParamIDs::trailDirection);
    flame                   = apvts.getRawParameterValue(ParamIDs::flame);
    shine                   = apvts.getRawParameterValue(ParamIDs::shine);
    gummy                   = apvts.getRawParameterValue(ParamIDs::gummy);
}
