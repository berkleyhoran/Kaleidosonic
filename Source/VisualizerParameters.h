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
    static const juce::String zoomWander             { "zoomWander" };
    static const juce::String cameraShake            { "cameraShake" };
    static const juce::String cameraScale            { "cameraScale" };
    static const juce::String palette                { "palette" };

    // Global post-FX, applied after whichever preset(s) render, regardless
    // of which preset is selected.
    static const juce::String trails                 { "trails" };
    static const juce::String blur                   { "blur" };
    static const juce::String noiseAmount             { "noiseAmount" };
    static const juce::String datamosh               { "datamosh" };
    static const juce::String bloomIntensity         { "bloomIntensity" };
    static const juce::String vignette               { "vignette" };
    static const juce::String chromaticAberration    { "chromaticAberration" };
    static const juce::String colorCycleSpeed        { "colorCycleSpeed" };
    static const juce::String pulseDepth             { "pulseDepth" };
    static const juce::String posterize              { "posterize" };
    static const juce::String fisheye                { "fisheye" };
    static const juce::String trailDirection         { "trailDirection" };
    static const juce::String flame                  { "flame" };
    static const juce::String shine                  { "shine" };
    static const juce::String gummy                  { "gummy" };
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
        "Tunnel Spiral",
        "Burning Ship",
        "Apollonian Gasket",
        "Raymarch Tunnel 3D",
        "Particle Bloom",
        "Oscilloscope Glow",
        "Waveform Scope",
        "Sierpinski Triforce",
        "Fractal Bubbles",
        "Starfield Warp",
        "Mandelbox",
        "Mandelbrot Explorer",
        "Burning Ship Explorer",
        "Perpendicular Ship",
        "Buffalo Fractal",
        "Tricorn",
        "Burning Ship 3D",
        "Audio Nebula"
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
    std::atomic<float>* zoomWander         = nullptr;
    std::atomic<float>* cameraShake        = nullptr;
    std::atomic<float>* cameraScale        = nullptr;
    std::atomic<float>* palette            = nullptr;

    std::atomic<float>* trails             = nullptr;
    std::atomic<float>* blur               = nullptr;
    std::atomic<float>* noiseAmount        = nullptr;
    std::atomic<float>* datamosh           = nullptr;
    std::atomic<float>* bloomIntensity     = nullptr;
    std::atomic<float>* vignette           = nullptr;
    std::atomic<float>* chromaticAberration = nullptr;
    std::atomic<float>* colorCycleSpeed    = nullptr;
    std::atomic<float>* pulseDepth         = nullptr;
    std::atomic<float>* posterize          = nullptr;
    std::atomic<float>* fisheye            = nullptr;
    std::atomic<float>* trailDirection     = nullptr;
    std::atomic<float>* flame              = nullptr;
    std::atomic<float>* shine              = nullptr;
    std::atomic<float>* gummy              = nullptr;

    void resolve(juce::AudioProcessorValueTreeState& apvts);
};
