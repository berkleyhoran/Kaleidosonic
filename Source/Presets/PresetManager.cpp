#include "PresetManager.h"
#include "BinaryData.h"

namespace
{
    struct Resource { const char* data; int size; };

    // Order must match PresetNames::all exactly.
    const Resource presetResources[] = {
        { BinaryData::mandelbrot_pulse_frag,   BinaryData::mandelbrot_pulse_fragSize },
        { BinaryData::julia_kaleidoscope_frag, BinaryData::julia_kaleidoscope_fragSize },
        { BinaryData::plasma_feedback_frag,    BinaryData::plasma_feedback_fragSize },
        { BinaryData::ifs_tunnel_frag,         BinaryData::ifs_tunnel_fragSize },
        { BinaryData::tunnel_spiral_frag,      BinaryData::tunnel_spiral_fragSize },
    };

    constexpr int numPresets = (int) (sizeof(presetResources) / sizeof(Resource));
}

int PresetManager::getNumPresets()
{
    return numPresets;
}

juce::String PresetManager::getVertexSource()
{
    return juce::String(BinaryData::fullscreen_vert, (size_t) BinaryData::fullscreen_vertSize);
}

juce::String PresetManager::getFragmentSource(int presetIndex)
{
    jassert(presetIndex >= 0 && presetIndex < numPresets);
    presetIndex = juce::jlimit(0, numPresets - 1, presetIndex);

    juce::String common(BinaryData::common_glsl, (size_t) BinaryData::common_glslSize);
    const auto& res = presetResources[(size_t) presetIndex];
    juce::String body(res.data, (size_t) res.size);

    return common + "\n" + body;
}
