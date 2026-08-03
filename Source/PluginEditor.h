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

private:
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
    juce::Viewport controlsViewport;
    juce::Component controlsContent;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    juce::OwnedArray<ParamSlider> paramSliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KaleidosonicAudioProcessorEditor)
};
