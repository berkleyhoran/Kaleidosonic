#include "VisualizerParameters.h"

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

    return { params.begin(), params.end() };
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
}
