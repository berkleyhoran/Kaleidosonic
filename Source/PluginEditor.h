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
// with dozens of parameters across 40+ presets, a flat list stopped being
// scannable. Whichever sliders the *current* preset's shader doesn't
// actually read (see isParamRelevantForPreset) are dimmed and disabled
// (not hidden -- the value is still real, still automatable from the DAW,
// and switching presets or morphing can make it matter again) so it's
// obvious at a glance what's worth touching right now. The Preset/Layer B
// dropdowns are further grouped into categories (see presetCategories())
// purely for scanability -- see populatePresetComboWithCategories.
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

    // A clickable color swatch: click it, a ColourSelector pops up in a
    // CallOutBox, and onColourChanged fires live as the user drags around
    // in it (not just on close) so the visual updates immediately.
    struct ColorSwatch : juce::Component, juce::ChangeListener
    {
        juce::Colour colour = juce::Colours::white;
        std::function<void(juce::Colour)> onColourChanged;

        void paint(juce::Graphics& g) override
        {
            g.fillAll(colour);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawRect(getLocalBounds());
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            auto selector = std::make_unique<juce::ColourSelector>(
                juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders
                | juce::ColourSelector::showColourspace);
            selector->setCurrentColour(colour);
            selector->setSize(260, 300);
            selector->addChangeListener(this);
            juce::CallOutBox::launchAsynchronously(std::move(selector), getScreenBounds(), nullptr);
        }

        void changeListenerCallback(juce::ChangeBroadcaster* source) override
        {
            if (auto* cs = dynamic_cast<juce::ColourSelector*>(source))
            {
                colour = cs->getCurrentColour();
                repaint();
                if (onColourChanged != nullptr)
                    onColourChanged(colour);
            }
        }
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

    // Fills a preset ComboBox (Preset / Layer B) with juce::ComboBox
    // section headings grouping presets by presetCategories(), each item's
    // ID pinned to its real PresetNames::all index + 1 -- see the
    // definition for why grouping can never desync the underlying flat
    // choice-parameter index.
    void populatePresetComboWithCategories(juce::ComboBox& box);

    // Builds an ID-based (not position-based) attachment between an APVTS
    // choice parameter and a ComboBox -- see the presetAttachment member's
    // comment for why the stock ComboBoxAttachment can't be used once
    // presets are grouped into categories.
    std::unique_ptr<juce::ParameterAttachment> makeIdBasedComboAttachment(const juce::String& paramID,
                                                                            juce::ComboBox& box);

    // "Load Image..." for the image-reactive presets (Image Ripple/Shatter/
    // Kaleidoscope). Opens an async FileChooser (required -- a modal one
    // would block the message thread), and on success hands the decoded
    // image to the renderer and remembers the path on the processor so it
    // reloads next time this editor opens (see PluginProcessor::
    // getImagePath/setImagePath).
    void openImageChooser();
    void loadImageFromFile(const juce::File& file);

    // Pushes a picked ColorSwatch colour into the three 0..1 R/G/B APVTS
    // params it's bound to (there's no single "colour" param type in
    // APVTS, so each swatch drives three plain floats).
    void pushColourToParams(const juce::Colour& colour, const juce::String& rId, const juce::String& gId,
                             const juce::String& bId);
    juce::Colour readColourFromParams(const juce::String& rId, const juce::String& gId, const juce::String& bId) const;

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
    // Independent of isFullscreenActive/controlsViewport -- just hides the
    // top-left "Controls"/"Fullscreen" buttons themselves, since fullscreen
    // mode already hides the side panel but left those two buttons sitting
    // on screen. Toggled with 'H'; there's no button for this one, since
    // hiding buttons via a button they'd then have no way to un-hide is
    // circular -- keyboard-only, like a few other hotkeys here.
    bool topBarHidden = false;
    juce::Point<float> lastDragPosition;
    int lastKnownPresetIndex = -1;

    juce::ComboBox presetBox;
    juce::Label presetLabel;
    // NOT a juce::AudioProcessorValueTreeState::ComboBoxAttachment -- that
    // class maps the parameter's value to comboBox.getSelectedItemIndex()/
    // setSelectedItemIndex() (POSITION among real items in insertion
    // order), completely ignoring the item ID passed to addItem(). Once
    // presets are grouped into categories (populatePresetComboWithCategories)
    // their insertion-order position no longer matches their real
    // PresetNames::all index, so that attachment class would silently
    // select the wrong preset -- confirmed against JUCE's own
    // ComboBoxParameterAttachment::setValue/comboBoxChanged source
    // (juce_ParameterAttachments.cpp), which is built entirely on
    // getSelectedItemIndex()/setSelectedItemIndex(). The generic
    // juce::ParameterAttachment below is wired by hand to use
    // getSelectedId()/setSelectedId() instead -- the ID IS explicitly set
    // to (real index + 1) in populatePresetComboWithCategories, so this is
    // the one that's actually safe to reorder/group.
    std::unique_ptr<juce::ParameterAttachment> presetAttachment;

    // Layer B / Blend Mode / Layer Mix: always visible, ungrouped, never
    // dimmed by updateParamRelevance -- same treatment as the Preset combo
    // above, since which preset(s) are showing isn't itself a per-preset-
    // relevant setting.
    juce::ComboBox layerBBox;
    juce::Label layerBLabel;
    // Same ID-based attachment as presetAttachment above, same reason.
    std::unique_ptr<juce::ParameterAttachment> layerBAttachment;
    juce::ComboBox blendModeBox;
    juce::Label blendModeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> blendModeAttachment;
    std::unique_ptr<ParamSlider> layerMixSlider;

    // Primary/Secondary color swatches -- also always visible/ungrouped,
    // with the Color Override amount slider that actually enables their
    // effect sitting directly underneath (moved out of the generic Color
    // group it used to live in, since that buried the one control that
    // makes these swatches do anything).
    ColorSwatch primaryColorSwatch, secondaryColorSwatch;
    juce::Label primaryColorLabel, secondaryColorLabel;
    std::unique_ptr<ParamSlider> colorOverrideSlider;

    juce::OwnedArray<ParamGroupUI> paramGroupUIs;

    juce::TextButton loadImageButton { "Load Image..." };
    juce::Label imageStatusLabel;
    std::unique_ptr<juce::FileChooser> imageFileChooser;

#if JUCE_WINDOWS
    // See SystemAudioLoopback.h -- lets the Standalone app visualize
    // whatever the system's speakers are currently playing without a
    // virtual audio cable. Only made visible when
    // processorRef.wrapperType is actually wrapperType_Standalone (a
    // *runtime* check -- see the big comment on
    // KaleidosonicAudioProcessor::setSystemAudioCaptureEnabled for why a
    // compile-time `#if JucePlugin_Build_Standalone` doesn't reliably
    // differ per format in this project's build setup), so it's absent
    // from a VST3 instance, which already gets its host track's audio
    // directly.
    juce::ToggleButton systemAudioToggle { "Capture System Audio (no virtual cable needed)" };
    bool showSystemAudioToggle = false;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KaleidosonicAudioProcessorEditor)
};
