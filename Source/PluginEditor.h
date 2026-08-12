#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Rendering/VisualizerRenderer.h"

// The visuals render as an OpenGL background across the whole editor; a
// collapsible panel of sliders sits on top so every automatable parameter
// can also be tweaked by hand (JUCE composites normal Component painting
// over an attached OpenGLContext, so this overlay costs nothing extra).
//
// The panel is organized into collapsible sections (see
// VisualizerParameters.h's paramGroups()) instead of one long flat list --
// with 35 parameters across 22 presets, a flat list stopped being
// scannable. Whichever sliders the *current* preset's shader doesn't
// actually read (see isParamRelevantForPreset) are dimmed and disabled
// (not hidden -- the value is still real, still automatable from the DAW,
// and switching presets or morphing can make it matter again) so it's
// obvious at a glance what's worth touching right now.
class KaleidosonicAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
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
        juce::String paramID;
    };

    // Clickable section header: title + a chevron that flips on click,
    // showing/hiding that section's sliders.
    struct GroupHeader : juce::Component
    {
        juce::String title;
        bool expanded = true;
        std::function<void()> onToggle;

        void paint(juce::Graphics& g) override;
        void mouseUp(const juce::MouseEvent&) override;
    };

    struct ParamGroupUI
    {
        GroupHeader header;
        juce::OwnedArray<ParamSlider> sliders;
    };

    ParamSlider* addParamSlider(juce::Component& parent, const juce::String& paramID,
                                 const juce::String& displayName);

    // "Load Image..." for the image-reactive presets (Image Ripple/Shatter/
    // Kaleidoscope). Opens an async FileChooser (required -- a modal one
    // would block the message thread), and on success hands the decoded
    // image to the renderer and remembers the path on the processor so it
    // reloads next time this editor opens (see PluginProcessor::
    // getImagePath/setImagePath).
    void openImageChooser();
    void loadImageFromFile(const juce::File& file);

    // Re-reads the current preset and dims/disables whichever sliders it
    // doesn't use. Called once at startup and on a light poll thereafter
    // (parameter changes can arrive from the audio thread via host
    // automation, so a lock-free atomic read + UI-thread poll is simpler
    // and safer here than an APVTS listener callback).
    void updateParamRelevance();
    void timerCallback() override;

    KaleidosonicAudioProcessor& processorRef;
    VisualizerRenderer renderer;

    juce::TextButton toggleControlsButton { "Controls" };
    juce::TextButton fullscreenButton { "Fullscreen" };
    juce::Viewport controlsViewport;
    ControlsPanel controlsContent;

    bool isFullscreenActive = false;
    juce::Point<float> lastDragPosition;
    int lastKnownPresetIndex = -1;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    std::unique_ptr<ParamSlider> presetMorphSlider; // always visible, ungrouped, never dimmed
    juce::OwnedArray<ParamGroupUI> paramGroupUIs;

    juce::TextButton loadImageButton { "Load Image..." };
    juce::Label imageStatusLabel;
    std::unique_ptr<juce::FileChooser> imageFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KaleidosonicAudioProcessorEditor)
};
