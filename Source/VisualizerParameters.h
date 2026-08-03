#pragma once

#include <JuceHeader.h>

// Central definition of every automatable parameter exposed to the host.
// IDs are the source of truth used by the processor, editor, and renderer.
namespace ParamIDs
{
    static const juce::String presetIndex          { "presetIndex" };
    static const juce::String presetMorph           { "presetMorph" };
    static const juce::String reactivity             { "reactivity" };
    static const juce::String bassGain               { "bassGain" };
    static const juce::String midGain                { "midGain" };
    static const juce::String trebleGain             { "trebleGain" };
    static const juce::String zoomSpeed              { "zoomSpeed" };
    static const juce::String rotationSpeed          { "rotationSpeed" };
    static const juce::String hue                    { "hue" };
    static const juce::String saturation             { "saturation" };
    static const juce::String brightness             { "brightness" };
    static const juce::String contrast               { "contrast" };
    static const juce::String kaleidoscopeSegments   { "kaleidoscopeSegments" };
    static const juce::String feedbackAmount         { "feedbackAmount" };
    static const juce::String iterations             { "iterations" };
    static const juce::String distortion             { "distortion" };
}

// Names of the built-in presets, in the order they are compiled/selected.
// Kept here (rather than only in PresetManager) so the APVTS choice
// parameter and the renderer's preset list can never drift apart.
namespace PresetNames
{
    static const juce::StringArray all {
        "Mandelbrot Pulse",
        "Julia Kaleidoscope",
        "Plasma Feedback",
        "IFS Tunnel",
        "Tunnel Spiral"
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Cached raw pointers into the APVTS for lock-free reads from the audio
// and render threads. Populated once after the APVTS is constructed.
struct VisualizerParameterRefs
{
    std::atomic<float>* presetIndex        = nullptr;
    std::atomic<float>* presetMorph        = nullptr;
    std::atomic<float>* reactivity         = nullptr;
    std::atomic<float>* bassGain           = nullptr;
    std::atomic<float>* midGain            = nullptr;
    std::atomic<float>* trebleGain         = nullptr;
    std::atomic<float>* zoomSpeed          = nullptr;
    std::atomic<float>* rotationSpeed      = nullptr;
    std::atomic<float>* hue                = nullptr;
    std::atomic<float>* saturation         = nullptr;
    std::atomic<float>* brightness         = nullptr;
    std::atomic<float>* contrast           = nullptr;
    std::atomic<float>* kaleidoscopeSegments = nullptr;
    std::atomic<float>* feedbackAmount     = nullptr;
    std::atomic<float>* iterations         = nullptr;
    std::atomic<float>* distortion         = nullptr;

    void resolve(juce::AudioProcessorValueTreeState& apvts);
};
