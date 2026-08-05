#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Rendering/VisualizerRenderer.h"

// The visuals render as an OpenGL background across the whole editor; a
// collapsible panel of sliders sits on top so every automatable parameter
// can also be tweaked by hand (JUCE composites normal Component painting
// over an attached OpenGLContext, so this overlay costs nothing extra).
class KaleidosonicAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit KaleidosonicAudioProcessorEditor(KaleidosonicAudioProcessor&);
    ~KaleidosonicAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Manual navigation for the explorer presets: wheel and up/down arrows
    // zoom, dragging the visual pans. Events over the controls panel never
    // reach these (the Viewport consumes them for scrolling instead).
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    void toggleFullscreen();

    // Opaque backdrop for the controls panel, so labels are readable over
    // any visual instead of floating transparently on top of it.
    struct ControlsPanel : juce::Component
    {
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xf0121219)); }
    };

    struct ParamSlider
    {
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addParamSlider(const juce::String& paramID, const juce::String& displayName);

    KaleidosonicAudioProcessor& processorRef;
    VisualizerRenderer renderer;

    juce::TextButton toggleControlsButton { "Controls" };
    juce::TextButton fullscreenButton { "Fullscreen" };
    juce::Viewport controlsViewport;
    ControlsPanel controlsContent;

    bool isFullscreenActive = false;
    juce::Point<float> lastDragPosition;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    juce::OwnedArray<ParamSlider> paramSliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KaleidosonicAudioProcessorEditor)
};
