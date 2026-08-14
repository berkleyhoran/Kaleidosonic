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
        { BinaryData::tunnel_spiral_frag,      BinaryData::tunnel_spiral_fragSize },
        { BinaryData::burning_ship_frag,       BinaryData::burning_ship_fragSize },
        { BinaryData::apollonian_frag,         BinaryData::apollonian_fragSize },
        { BinaryData::particle_bloom_frag,     BinaryData::particle_bloom_fragSize },
        { BinaryData::oscilloscope_frag,       BinaryData::oscilloscope_fragSize },
        { BinaryData::waveform_scope_frag,     BinaryData::waveform_scope_fragSize },
        { BinaryData::sierpinski_triforce_frag, BinaryData::sierpinski_triforce_fragSize },
        { BinaryData::fractal_bubbles_frag,    BinaryData::fractal_bubbles_fragSize },
        { BinaryData::starfield_warp_frag,     BinaryData::starfield_warp_fragSize },
        { BinaryData::mandelbox_frag,          BinaryData::mandelbox_fragSize },
        { BinaryData::mandelbrot_explorer_frag, BinaryData::mandelbrot_explorer_fragSize },
        { BinaryData::burning_ship_explorer_frag, BinaryData::burning_ship_explorer_fragSize },
        { BinaryData::perpendicular_ship_frag, BinaryData::perpendicular_ship_fragSize },
        { BinaryData::buffalo_frag,            BinaryData::buffalo_fragSize },
        { BinaryData::tricorn_frag,            BinaryData::tricorn_fragSize },
        { BinaryData::burning_ship_3d_frag,    BinaryData::burning_ship_3d_fragSize },
        { BinaryData::audio_nebula_frag,       BinaryData::audio_nebula_fragSize },
        { BinaryData::image_ripple_frag,       BinaryData::image_ripple_fragSize },
        { BinaryData::image_shatter_frag,      BinaryData::image_shatter_fragSize },
        { BinaryData::image_kaleidoscope_frag, BinaryData::image_kaleidoscope_fragSize },
        { BinaryData::shape_rave_frag,         BinaryData::shape_rave_fragSize },
        { BinaryData::infinite_maze_frag,      BinaryData::infinite_maze_fragSize },
        { BinaryData::light_logo_frag,         BinaryData::light_logo_fragSize },
        { BinaryData::wireframe_tunnel_frag,   BinaryData::wireframe_tunnel_fragSize },
        { BinaryData::metaballs_frag,          BinaryData::metaballs_fragSize },
        { BinaryData::crystal_cave_frag,       BinaryData::crystal_cave_fragSize },
        { BinaryData::spectrum_bars_frag,      BinaryData::spectrum_bars_fragSize },
        { BinaryData::radial_spectrum_frag,    BinaryData::radial_spectrum_fragSize },
        { BinaryData::stereo_field_frag,       BinaryData::stereo_field_fragSize },
        { BinaryData::wispy_ribbons_frag,      BinaryData::wispy_ribbons_fragSize },
        { BinaryData::neon_logo_frag,          BinaryData::neon_logo_fragSize },
        { BinaryData::logo_hologram_frag,      BinaryData::logo_hologram_fragSize },
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
