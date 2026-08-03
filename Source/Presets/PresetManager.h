#pragma once

#include <JuceHeader.h>

// Maps preset indices (matching PresetNames::all in VisualizerParameters.h)
// to their GLSL sources, pulled from BinaryData generated off Shaders/*.
// Every fragment source returned here has common.glsl's uniform block and
// helper functions prepended, so preset files only need to implement main().
namespace PresetManager
{
    int getNumPresets();
    juce::String getVertexSource();
    juce::String getFragmentSource(int presetIndex);
}
