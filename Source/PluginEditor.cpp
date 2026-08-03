#include "PluginEditor.h"

KaleidosonicAudioProcessorEditor::KaleidosonicAudioProcessorEditor(KaleidosonicAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), renderer(p.analyzer, p.paramRefs)
{
    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::left);
    controlsContent.addAndMakeVisible(presetLabel);

    presetBox.addItemList(PresetNames::all, 1);
    controlsContent.addAndMakeVisible(presetBox);
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, ParamIDs::presetIndex, presetBox);

    addParamSlider(ParamIDs::presetMorph, "Morph To Next");
    addParamSlider(ParamIDs::reactivity, "Reactivity");
    addParamSlider(ParamIDs::bassGain, "Bass Gain");
    addParamSlider(ParamIDs::midGain, "Mid Gain");
    addParamSlider(ParamIDs::trebleGain, "Treble Gain");
    addParamSlider(ParamIDs::zoomSpeed, "Zoom Speed");
    addParamSlider(ParamIDs::rotationSpeed, "Rotation Speed");
    addParamSlider(ParamIDs::hue, "Hue");
    addParamSlider(ParamIDs::saturation, "Saturation");
    addParamSlider(ParamIDs::brightness, "Brightness");
    addParamSlider(ParamIDs::contrast, "Contrast");
    addParamSlider(ParamIDs::kaleidoscopeSegments, "Kaleidoscope Segments");
    addParamSlider(ParamIDs::feedbackAmount, "Feedback");
    addParamSlider(ParamIDs::iterations, "Iterations");
    addParamSlider(ParamIDs::distortion, "Distortion");

    controlsViewport.setViewedComponent(&controlsContent, false);
    controlsViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(controlsViewport);

    toggleControlsButton.setClickingTogglesState(true);
    toggleControlsButton.setToggleState(true, juce::dontSendNotification);
    toggleControlsButton.onClick = [this]
    {
        controlsViewport.setVisible(toggleControlsButton.getToggleState());
        resized();
    };
    addAndMakeVisible(toggleControlsButton);

    setResizable(true, true);
    setResizeLimits(480, 360, 3840, 2160);
    setSize(900, 650);

    renderer.attachTo(*this);
}

KaleidosonicAudioProcessorEditor::~KaleidosonicAudioProcessorEditor()
{
    renderer.detach();
}

void KaleidosonicAudioProcessorEditor::addParamSlider(const juce::String& paramID, const juce::String& displayName)
{
    auto* ps = new ParamSlider();
    ps->label.setText(displayName, juce::dontSendNotification);
    ps->label.setJustificationType(juce::Justification::left);
    ps->label.setColour(juce::Label::textColourId, juce::Colours::white);
    controlsContent.addAndMakeVisible(ps->label);

    ps->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    controlsContent.addAndMakeVisible(ps->slider);

    ps->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, paramID, ps->slider);

    paramSliders.add(ps);
}

void KaleidosonicAudioProcessorEditor::paint(juce::Graphics&)
{
    // Background is the OpenGL-rendered visualizer; nothing to paint here.
}

void KaleidosonicAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    toggleControlsButton.setBounds(bounds.removeFromTop(28).removeFromLeft(90).reduced(2));

    if (! controlsViewport.isVisible())
        return;

    const int panelWidth = juce::jmin(300, getWidth() / 2);
    auto panelBounds = getLocalBounds().removeFromRight(panelWidth);
    controlsViewport.setBounds(panelBounds);

    const int rowHeight = 46;
    const int numRows = 1 + paramSliders.size(); // preset combo + sliders
    controlsContent.setSize(panelBounds.getWidth() - controlsViewport.getScrollBarThickness(),
                             rowHeight * numRows + 16);

    auto content = controlsContent.getLocalBounds().reduced(8);
    auto presetRow = content.removeFromTop(rowHeight);
    presetLabel.setBounds(presetRow.removeFromTop(18));
    presetBox.setBounds(presetRow);

    for (auto* ps : paramSliders)
    {
        auto row = content.removeFromTop(rowHeight);
        ps->label.setBounds(row.removeFromTop(18));
        ps->slider.setBounds(row);
    }
}
