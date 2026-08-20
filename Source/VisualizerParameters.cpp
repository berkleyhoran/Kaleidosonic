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

    // Layer B defaults to the second preset in the list (index 1, if there
    // is one) purely so it's never identical to Layer A's default -- it
    // doesn't matter in practice since Layer Mix defaults to 0 (Layer A
    // only) until the user actually turns layering on.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::layerBIndex, 1), "Layer B", PresetNames::all,
        juce::jmin(1, PresetNames::all.size() - 1)));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(ParamIDs::blendMode, 1), "Blend Mode", BlendModeNames::all, 0));
    params.push_back(makeFloat(ParamIDs::layerMix, "Layer Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::reactivity, "Reactivity", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::bassGain, "Bass Gain", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::midGain, "Mid Gain", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::trebleGain, "Treble Gain", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::eqLowGain, "EQ Low", -18.0f, 18.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::eqMidGain, "EQ Mid", -18.0f, 18.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::eqHighGain, "EQ High", -18.0f, 18.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::particleDensity, "Particle Density", 0.0f, 2.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::particleSize, "Particle Size", 0.0f, 2.0f, 1.0f));
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
    params.push_back(makeFloat(ParamIDs::jpegify, "Jpegify", 0.0f, 1.0f, 0.0f));
    params.push_back(makeFloat(ParamIDs::dotMatrix, "Dot Matrix", 0.0f, 1.0f, 0.0f));

    params.push_back(makeFloat(ParamIDs::colorOverride, "Color Override", 0.0f, 1.0f, 0.0f));
    // Not exposed as sliders (see PluginEditor's ColorSwatch) -- a synthwave
    // pink/blue default so the duotone looks intentional the moment someone
    // turns Color Override up without having picked colors first.
    params.push_back(makeFloat(ParamIDs::primaryColorR, "Primary Color R", 0.0f, 1.0f, 1.0f));
    params.push_back(makeFloat(ParamIDs::primaryColorG, "Primary Color G", 0.0f, 1.0f, 0.15f));
    params.push_back(makeFloat(ParamIDs::primaryColorB, "Primary Color B", 0.0f, 1.0f, 0.65f));
    params.push_back(makeFloat(ParamIDs::secondaryColorR, "Secondary Color R", 0.0f, 1.0f, 0.05f));
    params.push_back(makeFloat(ParamIDs::secondaryColorG, "Secondary Color G", 0.0f, 1.0f, 0.25f));
    params.push_back(makeFloat(ParamIDs::secondaryColorB, "Secondary Color B", 0.0f, 1.0f, 0.95f));

    return { params.begin(), params.end() };
}

const std::vector<ParamGroupInfo>& paramGroups()
{
    static const std::vector<ParamGroupInfo> groups {
        { "Audio Reactivity", { ParamIDs::reactivity, ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain } },
        // A real EQ on the signal itself, applied before FFT/band analysis
        // (see AudioAnalyzer::setEqGains) -- distinct from the Gain sliders
        // above, which only rescale the already-analyzed bass/mid/treble
        // numbers. Its own group since it's a genuinely different kind of
        // control (dB, pre-analysis) from everything else here.
        { "Input EQ (Pre-Analysis)", { ParamIDs::eqLowGain, ParamIDs::eqMidGain, ParamIDs::eqHighGain } },
        // Only relevant for the three particle-field presets (Particle
        // Bloom, Starfield Warp, Fractal Bubbles) -- see conditionalRelevance.
        { "Particles", { ParamIDs::particleDensity, ParamIDs::particleSize } },
        { "Motion & Zoom", { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::cameraShake,
                              ParamIDs::cameraScale, ParamIDs::zoomWander } },
        { "Fractal Detail", { ParamIDs::iterations, ParamIDs::kaleidoscopeSegments, ParamIDs::distortion,
                               ParamIDs::feedbackAmount } },
        // colorOverride is deliberately NOT listed here -- it lives as its
        // own always-visible slider directly under the Primary/Secondary
        // Color swatches in the editor (same treatment as Layer Mix under
        // the Layer B combo), since that's where it conceptually belongs
        // and it's the one slider that actually enables what those
        // swatches do.
        { "Color", { ParamIDs::hue, ParamIDs::saturation, ParamIDs::brightness, ParamIDs::contrast,
                      ParamIDs::palette } },
        { "Post FX: Trails & Flame", { ParamIDs::trails, ParamIDs::trailDirection, ParamIDs::flame } },
        { "Post FX: Filters", { ParamIDs::blur, ParamIDs::noiseAmount, ParamIDs::datamosh, ParamIDs::shine,
                                 ParamIDs::gummy, ParamIDs::posterize, ParamIDs::fisheye,
                                 ParamIDs::chromaticAberration, ParamIDs::jpegify, ParamIDs::dotMatrix } },
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
            /* 3  Tunnel Spiral -- smooth constant zoom now, Camera Shake no longer read */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraScale },
            /* 4  Burning Ship */
            { ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments,
              ParamIDs::iterations, ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::cameraScale,
              ParamIDs::palette },
            /* 5  Apollonian Gasket */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed,
              ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments, ParamIDs::distortion, ParamIDs::zoomWander,
              ParamIDs::cameraShake, ParamIDs::cameraScale },
            /* 6  Particle Bloom */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::cameraScale, ParamIDs::particleDensity,
              ParamIDs::particleSize },
            /* 7  Oscilloscope Glow */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::kaleidoscopeSegments, ParamIDs::distortion },
            /* 8  Waveform Scope */
            { ParamIDs::trebleGain },
            /* 9  Sierpinski Triforce */
            { ParamIDs::bassGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 10 Fractal Bubbles */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed,
              ParamIDs::zoomWander, ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::particleDensity,
              ParamIDs::particleSize },
            /* 11 Starfield Warp */
            { ParamIDs::bassGain, ParamIDs::zoomSpeed, ParamIDs::zoomWander, ParamIDs::cameraShake,
              ParamIDs::cameraScale, ParamIDs::particleDensity, ParamIDs::particleSize },
            /* 12 Mandelbox */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::iterations, ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            // 13-17: the manual explorers all share exploreFractal() in
            // common.glsl, which now adds a slight always-on Rotation
            // Speed/Zoom Speed/Distortion drift on top of the manual view
            // (see its own comment for why this doesn't fight manual nav).
            /* 13 Mandelbrot Explorer */
            { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 14 Burning Ship Explorer */
            { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 15 Perpendicular Ship */
            { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 16 Buffalo Fractal */
            { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 17 Tricorn */
            { ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 18 Burning Ship 3D */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::rotationSpeed, ParamIDs::iterations,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 19 Audio Nebula */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed, ParamIDs::distortion,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 20 Image Ripple */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::distortion,
              ParamIDs::cameraScale, ParamIDs::palette },
            /* 21 Image Shatter */
            { ParamIDs::bassGain, ParamIDs::distortion, ParamIDs::cameraScale, ParamIDs::palette },
            /* 22 Image Kaleidoscope */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed, ParamIDs::kaleidoscopeSegments,
              ParamIDs::zoomWander, ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 23 Shape Rave */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed, ParamIDs::zoomSpeed,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 24 Infinite Maze -- Zoom Speed/Camera Shake repurposed as walk speed/turn eagerness */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::distortion, ParamIDs::zoomSpeed,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 25 Wireframe Tunnel */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::cameraScale },
            /* 26 Metaballs */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::rotationSpeed, ParamIDs::cameraShake,
              ParamIDs::cameraScale, ParamIDs::palette },
            /* 27 Crystal Cave */
            { ParamIDs::bassGain, ParamIDs::rotationSpeed, ParamIDs::cameraShake, ParamIDs::cameraScale,
              ParamIDs::palette },
            /* 28 Spectrum Bars -- reads uSpectrum directly, not uBass/uMid/uTreble, so no gain sliders */
            { ParamIDs::palette },
            /* 29 Radial Spectrum -- Camera Scale zooms the whole ring, Rotation Speed spins it;
               also reads uSpectrum directly, not uBass/uMid/uTreble, so no gain sliders */
            { ParamIDs::rotationSpeed, ParamIDs::cameraScale, ParamIDs::palette },
            /* 30 Stereo Field -- reads uStereoScope/uCorrelation directly, not uBass/uMid/uTreble */
            { ParamIDs::cameraScale, ParamIDs::palette },
            /* 31 Wispy Ribbons -- Zoom Speed repurposed as horizontal drift speed */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed,
              ParamIDs::cameraScale, ParamIDs::palette },
            /* 32 Video Feedback -- Zoom Speed=Growth, Rotation Speed=Rotation,
               Distortion=Direction, Feedback Amount=Blend (see the .frag header) */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::distortion, ParamIDs::feedbackAmount, ParamIDs::cameraShake, ParamIDs::cameraScale,
              ParamIDs::palette },
            /* 33 Tri-Color Waves */
            { ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::distortion },
            /* 34 Wave Sphere */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed,
              ParamIDs::distortion, ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 35 Image Fragments */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::rotationSpeed, ParamIDs::distortion,
              ParamIDs::cameraScale, ParamIDs::palette },
            /* 36 Image Feedback Zoom */
            { ParamIDs::bassGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::feedbackAmount,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 37 Ocean Floor -- deliberately not audio-reactive (see the .frag header);
               Zoom Speed repurposed as current drift speed */
            { ParamIDs::zoomSpeed, ParamIDs::cameraScale, ParamIDs::palette },
            /* 38 Water Ripples -- deliberately not audio-reactive (see the .frag header) */
            { ParamIDs::distortion, ParamIDs::cameraScale, ParamIDs::palette },
            /* 39 Jelly Polygons -- calm drifting now, no movement-based reactivity (Bass/Mid Gain no longer read) */
            { ParamIDs::rotationSpeed, ParamIDs::cameraScale, ParamIDs::palette },
            /* 40 Goopy Slime */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 41 Bouncing Shapes -- fully 2D now, bounces off the actual screen edges and off each other */
            { ParamIDs::bassGain, ParamIDs::midGain, ParamIDs::trebleGain, ParamIDs::rotationSpeed,
              ParamIDs::cameraShake, ParamIDs::cameraScale, ParamIDs::palette },
            /* 42 2D Flames -- Zoom Speed repurposed as upward rise/flicker speed */
            { ParamIDs::bassGain, ParamIDs::trebleGain, ParamIDs::zoomSpeed, ParamIDs::distortion,
              ParamIDs::palette },
            /* 43 Energy Tunnel -- Zoom Speed=travel speed, Rotation Speed=spin around the tunnel axis */
            { ParamIDs::bassGain, ParamIDs::zoomSpeed, ParamIDs::rotationSpeed, ParamIDs::iterations,
              ParamIDs::cameraShake, ParamIDs::palette },
            /* 44 Terrain Flyover -- deliberately calm/not audio-reactive (see the .frag header);
               Zoom Speed=travel speed (manual, not reactivity) */
            { ParamIDs::zoomSpeed, ParamIDs::cameraScale, ParamIDs::palette },
        };
        return table;
    }
}

const std::vector<PresetCategory>& presetCategories()
{
    // Every name here must exist in PresetNames::all -- the editor looks
    // each one up by indexOf() rather than assuming these ranges are
    // contiguous in the flat list, so presets can be reshuffled between
    // categories here freely without ever touching the real APVTS index
    // any of them lives at.
    static const std::vector<PresetCategory> categories {
        { "Fractal Dives", { "Mandelbrot Pulse", "Julia Kaleidoscope", "Burning Ship", "Apollonian Gasket",
                              "Sierpinski Triforce" } },
        { "Fractal Explorers", { "Mandelbrot Explorer", "Burning Ship Explorer", "Perpendicular Ship",
                                  "Buffalo Fractal", "Tricorn" } },
        { "Fractal 3D", { "Burning Ship 3D", "Mandelbox", "Energy Tunnel" } },
        { "Tunnels & Feedback", { "Tunnel Spiral", "Plasma Feedback", "Wireframe Tunnel", "Infinite Maze",
                                   "Video Feedback" } },
        { "Particles & Fields", { "Particle Bloom", "Starfield Warp", "Fractal Bubbles", "Audio Nebula",
                                   "Shape Rave", "Metaballs", "Crystal Cave", "Wispy Ribbons", "2D Flames" } },
        { "Waveforms & Analyzers", { "Oscilloscope Glow", "Waveform Scope", "Spectrum Bars", "Radial Spectrum",
                                      "Stereo Field", "Tri-Color Waves", "Wave Sphere" } },
        { "Image Reactive", { "Image Ripple", "Image Shatter", "Image Kaleidoscope", "Image Fragments",
                               "Image Feedback Zoom" } },
        { "Environments", { "Ocean Floor", "Water Ripples", "Terrain Flyover" } },
        { "Organic & Physics", { "Jelly Polygons", "Goopy Slime", "Bouncing Shapes" } },
    };
    return categories;
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
        ParamIDs::gummy, ParamIDs::colorOverride, ParamIDs::jpegify, ParamIDs::dotMatrix,
        // EQ reshapes the signal before analysis, upstream of every preset
        // that reads any audio-driven uniform at all -- always relevant for
        // the same reason Reactivity is, regardless of which specific band
        // uniforms a given preset's shader happens to read.
        ParamIDs::eqLowGain, ParamIDs::eqMidGain, ParamIDs::eqHighGain,
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
    layerBIndex           = apvts.getRawParameterValue(ParamIDs::layerBIndex);
    blendMode             = apvts.getRawParameterValue(ParamIDs::blendMode);
    layerMix               = apvts.getRawParameterValue(ParamIDs::layerMix);
    reactivity            = apvts.getRawParameterValue(ParamIDs::reactivity);
    bassGain              = apvts.getRawParameterValue(ParamIDs::bassGain);
    midGain                = apvts.getRawParameterValue(ParamIDs::midGain);
    trebleGain             = apvts.getRawParameterValue(ParamIDs::trebleGain);
    eqLowGain               = apvts.getRawParameterValue(ParamIDs::eqLowGain);
    eqMidGain                = apvts.getRawParameterValue(ParamIDs::eqMidGain);
    eqHighGain               = apvts.getRawParameterValue(ParamIDs::eqHighGain);
    particleDensity          = apvts.getRawParameterValue(ParamIDs::particleDensity);
    particleSize             = apvts.getRawParameterValue(ParamIDs::particleSize);
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
    jpegify                 = apvts.getRawParameterValue(ParamIDs::jpegify);
    dotMatrix               = apvts.getRawParameterValue(ParamIDs::dotMatrix);

    colorOverride           = apvts.getRawParameterValue(ParamIDs::colorOverride);
    primaryColorR           = apvts.getRawParameterValue(ParamIDs::primaryColorR);
    primaryColorG           = apvts.getRawParameterValue(ParamIDs::primaryColorG);
    primaryColorB           = apvts.getRawParameterValue(ParamIDs::primaryColorB);
    secondaryColorR         = apvts.getRawParameterValue(ParamIDs::secondaryColorR);
    secondaryColorG         = apvts.getRawParameterValue(ParamIDs::secondaryColorG);
    secondaryColorB         = apvts.getRawParameterValue(ParamIDs::secondaryColorB);
}
