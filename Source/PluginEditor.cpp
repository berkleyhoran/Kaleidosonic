#include "PluginEditor.h"
#include "Rendering/GifDecoder.h"

void KaleidosonicAudioProcessorEditor::GroupHeader::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour(juce::Colour(0xff1e1526));
    g.fillRect(bounds);
    g.setColour(juce::Colour(0xff382a44));
    g.fillRect(bounds.removeFromBottom(1));

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    auto textBounds = getLocalBounds().reduced(8, 0);
    g.drawText((expanded ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xbe ")) // ▾
                          : juce::String(juce::CharPointer_UTF8("\xe2\x96\xb8 "))) // ▸
                   + title,
               textBounds, juce::Justification::centredLeft);
}

void KaleidosonicAudioProcessorEditor::GroupHeader::mouseUp(const juce::MouseEvent&)
{
    expanded = ! expanded;
    repaint();
    if (onToggle != nullptr)
        onToggle();
}

KaleidosonicAudioProcessorEditor::KaleidosonicAudioProcessorEditor(KaleidosonicAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), renderer(p.analyzer, p.paramRefs)
{
    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::left);
    presetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    controlsContent.addAndMakeVisible(presetLabel);

    presetBox.addItemList(PresetNames::all, 1);
    controlsContent.addAndMakeVisible(presetBox);
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, ParamIDs::presetIndex, presetBox);

    layerBLabel.setText("Layer B", juce::dontSendNotification);
    layerBLabel.setJustificationType(juce::Justification::left);
    layerBLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    controlsContent.addAndMakeVisible(layerBLabel);
    layerBBox.addItemList(PresetNames::all, 1);
    controlsContent.addAndMakeVisible(layerBBox);
    layerBAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, ParamIDs::layerBIndex, layerBBox);

    blendModeLabel.setText("Blend Mode", juce::dontSendNotification);
    blendModeLabel.setJustificationType(juce::Justification::left);
    blendModeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    controlsContent.addAndMakeVisible(blendModeLabel);
    blendModeBox.addItemList(BlendModeNames::all, 1);
    controlsContent.addAndMakeVisible(blendModeBox);
    blendModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, ParamIDs::blendMode, blendModeBox);

    layerMixSlider.reset(addParamSlider(controlsContent, ParamIDs::layerMix, "Layer Mix"));

    primaryColorLabel.setText("Primary Color", juce::dontSendNotification);
    primaryColorLabel.setJustificationType(juce::Justification::left);
    primaryColorLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    controlsContent.addAndMakeVisible(primaryColorLabel);
    primaryColorSwatch.colour = readColourFromParams(ParamIDs::primaryColorR, ParamIDs::primaryColorG,
                                                       ParamIDs::primaryColorB);
    primaryColorSwatch.onColourChanged = [this](juce::Colour c)
    { pushColourToParams(c, ParamIDs::primaryColorR, ParamIDs::primaryColorG, ParamIDs::primaryColorB); };
    controlsContent.addAndMakeVisible(primaryColorSwatch);

    secondaryColorLabel.setText("Secondary Color", juce::dontSendNotification);
    secondaryColorLabel.setJustificationType(juce::Justification::left);
    secondaryColorLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    controlsContent.addAndMakeVisible(secondaryColorLabel);
    secondaryColorSwatch.colour = readColourFromParams(ParamIDs::secondaryColorR, ParamIDs::secondaryColorG,
                                                         ParamIDs::secondaryColorB);
    secondaryColorSwatch.onColourChanged = [this](juce::Colour c)
    { pushColourToParams(c, ParamIDs::secondaryColorR, ParamIDs::secondaryColorG, ParamIDs::secondaryColorB); };
    controlsContent.addAndMakeVisible(secondaryColorSwatch);

    loadImageButton.onClick = [this] { openImageChooser(); };
    controlsContent.addAndMakeVisible(loadImageButton);
    imageStatusLabel.setJustificationType(juce::Justification::left);
    imageStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    imageStatusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    imageStatusLabel.setText("No image loaded", juce::dontSendNotification);
    controlsContent.addAndMakeVisible(imageStatusLabel);

    // Reconnect a picture chosen in an earlier session (the path survives
    // in the plugin's saved state -- see PluginProcessor::getImagePath).
    // setSourceImage() is safe to call before the GL context exists; the
    // upload is deferred to the first frame either way.
    if (const auto savedPath = processorRef.getImagePath(); savedPath.isNotEmpty())
    {
        if (juce::File file(savedPath); file.existsAsFile())
            loadImageFromFile(file);
    }

    for (const auto& groupInfo : paramGroups())
    {
        auto* groupUI = new ParamGroupUI();
        groupUI->header.title = groupInfo.title;
        groupUI->header.onToggle = [this] { resized(); };
        controlsContent.addAndMakeVisible(groupUI->header);

        for (const auto& paramID : groupInfo.paramIDs)
        {
            // Display name: look up the human-readable name JUCE already
            // has from the parameter itself, rather than a second
            // hand-maintained ID->name table that could drift out of sync.
            juce::String displayName = paramID;
            if (auto* param = processorRef.apvts.getParameter(paramID))
                displayName = param->getName(64);

            groupUI->sliders.add(addParamSlider(controlsContent, paramID, displayName));
        }

        paramGroupUIs.add(groupUI);
    }

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

    fullscreenButton.onClick = [this] { toggleFullscreen(); };
    addAndMakeVisible(fullscreenButton);

    setResizable(true, true);
    setResizeLimits(480, 360, 3840, 2160);
    setSize(900, 650);
    setWantsKeyboardFocus(true);

    renderer.attachTo(*this);

    updateParamRelevance();
    startTimerHz(6);
}

KaleidosonicAudioProcessorEditor::~KaleidosonicAudioProcessorEditor()
{
    stopTimer();
    renderer.detach();
}

KaleidosonicAudioProcessorEditor::ParamSlider* KaleidosonicAudioProcessorEditor::addParamSlider(
    juce::Component& parent, const juce::String& paramID, const juce::String& displayName)
{
    auto* ps = new ParamSlider();
    ps->paramID = paramID;
    ps->label.setText(displayName, juce::dontSendNotification);
    ps->label.setJustificationType(juce::Justification::left);
    ps->label.setColour(juce::Label::textColourId, juce::Colours::white);
    parent.addAndMakeVisible(ps->label);

    ps->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    // Sliders must not swallow the mouse wheel: the wheel scrolls the panel
    // (or zooms an explorer preset), and accidentally yanking parameter
    // values while scrolling was a genuine usability bug.
    ps->slider.setScrollWheelEnabled(false);
    parent.addAndMakeVisible(ps->slider);

    ps->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, paramID, ps->slider);

    return ps;
}

void KaleidosonicAudioProcessorEditor::openImageChooser()
{
    imageFileChooser = std::make_unique<juce::FileChooser>(
        "Choose an image for the image-reactive presets", juce::File(), "*.png;*.jpg;*.jpeg;*.bmp;*.gif");

    // launchAsync, not the old blocking show(): a modal file dialog on the
    // message thread is deprecated in modern JUCE and disallowed outright
    // in some sandboxed hosts. The chooser stays alive on `this` until the
    // callback fires.
    imageFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& fc)
                                   {
                                       const auto file = fc.getResult();
                                       if (file.existsAsFile())
                                           loadImageFromFile(file);
                                   });
}

void KaleidosonicAudioProcessorEditor::loadImageFromFile(const juce::File& file)
{
    // GIFs need their own path: JUCE's own ImageFileFormat only ever
    // decodes a GIF's first frame (no animation API), so animated
    // playback goes through the vendored stb_image decoder instead (see
    // GifDecoder.h) -- everything else (PNG/JPG/BMP) still goes through
    // JUCE as before, which is the lighter-weight/better-tested path for
    // those formats.
    if (file.hasFileExtension("gif"))
    {
        juce::MemoryBlock raw;
        if (! file.loadFileAsData(raw))
        {
            imageStatusLabel.setText("Couldn't read \"" + file.getFileName() + "\"", juce::dontSendNotification);
            return;
        }

        auto gif = decodeGif(static_cast<const uint8_t*>(raw.getData()), raw.getSize());
        if (! gif.ok)
        {
            imageStatusLabel.setText("Couldn't decode \"" + file.getFileName() + "\"", juce::dontSendNotification);
            return;
        }

        const int numFrames = (int) gif.frames.size();
        renderer.setSourceImageAnimated(std::move(gif.frames), std::move(gif.frameDelaysMs), gif.width, gif.height);
        processorRef.setImagePath(file.getFullPathName());
        imageStatusLabel.setText(file.getFileName()
                                      + (numFrames > 1 ? " (" + juce::String(numFrames) + " frames)" : ""),
                                  juce::dontSendNotification);
        return;
    }

    const auto image = juce::ImageFileFormat::loadFrom(file);
    if (! image.isValid())
    {
        imageStatusLabel.setText("Couldn't read \"" + file.getFileName() + "\"", juce::dontSendNotification);
        return;
    }

    renderer.setSourceImage(image);
    processorRef.setImagePath(file.getFullPathName());
    imageStatusLabel.setText(file.getFileName(), juce::dontSendNotification);
}

void KaleidosonicAudioProcessorEditor::pushColourToParams(const juce::Colour& colour, const juce::String& rId,
                                                            const juce::String& gId, const juce::String& bId)
{
    auto setOne = [this](const juce::String& id, float value)
    {
        if (auto* param = processorRef.apvts.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };
    setOne(rId, colour.getFloatRed());
    setOne(gId, colour.getFloatGreen());
    setOne(bId, colour.getFloatBlue());
}

juce::Colour KaleidosonicAudioProcessorEditor::readColourFromParams(const juce::String& rId, const juce::String& gId,
                                                                      const juce::String& bId) const
{
    auto readOne = [this](const juce::String& id, float fallback)
    {
        auto* raw = processorRef.apvts.getRawParameterValue(id);
        return raw != nullptr ? raw->load() : fallback;
    };
    return juce::Colour::fromFloatRGBA(readOne(rId, 1.0f), readOne(gId, 1.0f), readOne(bId, 1.0f), 1.0f);
}

void KaleidosonicAudioProcessorEditor::timerCallback()
{
    const int current = processorRef.paramRefs.presetIndex != nullptr
                             ? juce::roundToInt(processorRef.paramRefs.presetIndex->load())
                             : 0;
    if (current != lastKnownPresetIndex)
        updateParamRelevance();
}

void KaleidosonicAudioProcessorEditor::updateParamRelevance()
{
    lastKnownPresetIndex = processorRef.paramRefs.presetIndex != nullptr
                                ? juce::roundToInt(processorRef.paramRefs.presetIndex->load())
                                : 0;

    constexpr float dimAlpha = 0.38f;
    for (auto* groupUI : paramGroupUIs)
    {
        for (auto* ps : groupUI->sliders)
        {
            const bool relevant = isParamRelevantForPreset(lastKnownPresetIndex, ps->paramID);
            ps->label.setAlpha(relevant ? 1.0f : dimAlpha);
            ps->slider.setAlpha(relevant ? 1.0f : dimAlpha);
            // Dimmed but not functionally locked out: the parameter is
            // still real and still automatable from the DAW regardless
            // (host automation doesn't go through this UI at all), so
            // disabling local mouse interaction would only stop a user
            // from pre-setting a value ahead of switching to a preset
            // where it matters -- not worth the confusion of a truly dead
            // control.
        }
    }
}

void KaleidosonicAudioProcessorEditor::paint(juce::Graphics&)
{
    // Background is the OpenGL-rendered visualizer; nothing to paint here.
}

bool KaleidosonicAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        toggleFullscreen();
        return true;
    }
    if (key == juce::KeyPress::escapeKey && isFullscreenActive)
    {
        toggleFullscreen();
        return true;
    }
    // 'H' hides/shows just the top-left Controls/Fullscreen buttons --
    // independent of 'F', since fullscreen already hides the side panel but
    // leaves those two buttons on screen.
    if (key.getTextCharacter() == 'h' || key.getTextCharacter() == 'H')
    {
        topBarHidden = ! topBarHidden;
        toggleControlsButton.setVisible(! topBarHidden);
        fullscreenButton.setVisible(! topBarHidden);
        resized();
        return true;
    }
    // Up/down arrows zoom the explorer presets (no-ops elsewhere).
    if (key == juce::KeyPress::upKey)
    {
        renderer.requestManualZoom(3.0f);
        return true;
    }
    if (key == juce::KeyPress::downKey)
    {
        renderer.requestManualZoom(-3.0f);
        return true;
    }
    // [ / ] step through presets -- handy live, and matches the combobox.
    const auto ch = key.getTextCharacter();
    if (ch == '[' || ch == ']')
    {
        if (auto* param = processorRef.apvts.getParameter(ParamIDs::presetIndex))
        {
            const int numPresets = PresetNames::all.size();
            const int current = juce::roundToInt(param->convertFrom0to1(param->getValue()));
            const int next = juce::jlimit(0, numPresets - 1, current + (ch == ']' ? 1 : -1));
            param->setValueNotifyingHost(param->convertTo0to1((float) next));
        }
        return true;
    }
    return false;
}

void KaleidosonicAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& event,
                                                      const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);
    // Wheel over the visual zooms the explorer presets. (Events over the
    // controls panel are consumed by the Viewport for scrolling and never
    // reach here.)
    renderer.requestManualZoom(wheel.deltaY * 12.0f);
}

void KaleidosonicAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    lastDragPosition = event.position;
}

void KaleidosonicAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    const auto delta = event.position - lastDragPosition;
    lastDragPosition = event.position;
    const float h = (float) juce::jmax(getHeight(), 1);
    // Screen-pixel drag deltas as fractions of the view height; the
    // renderer flips signs so the content follows the cursor.
    renderer.requestManualPan(delta.x / h, delta.y / h);
}

void KaleidosonicAudioProcessorEditor::toggleFullscreen()
{
    // Uses JUCE's native kiosk mode, which saves/restores the component's
    // bounds itself. Works reliably for the Standalone app; VST3 hosts vary
    // in how much they let a plugin editor's top-level window move/resize,
    // so treat this as "usually works, host-dependent" rather than
    // guaranteed true OS fullscreen.
    auto* top = getTopLevelComponent();
    if (top == nullptr)
        return;

    auto& desktop = juce::Desktop::getInstance();
    if (! isFullscreenActive)
    {
        desktop.setKioskModeComponent(top, false);
        controlsViewport.setVisible(false);
        toggleControlsButton.setToggleState(false, juce::dontSendNotification);
        isFullscreenActive = true;
    }
    else
    {
        desktop.setKioskModeComponent(nullptr);
        isFullscreenActive = false;
    }

    resized();
}

void KaleidosonicAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    if (! topBarHidden)
    {
        auto topBar = bounds.removeFromTop(28);
        toggleControlsButton.setBounds(topBar.removeFromLeft(90).reduced(2));
        fullscreenButton.setBounds(topBar.removeFromLeft(90).reduced(2));
    }

    if (! controlsViewport.isVisible())
        return;

    const int panelWidth = juce::jmin(320, getWidth() / 2);
    auto panelBounds = getLocalBounds().removeFromRight(panelWidth);
    controlsViewport.setBounds(panelBounds);

    constexpr int rowHeight = 44;
    constexpr int headerHeight = 26;
    constexpr int presetRowHeight = 46;
    constexpr int imageRowHeight = 46;
    constexpr int colorRowHeight = 46;

    // First pass: total content height, so the Viewport's scrollbar is
    // sized correctly before we position anything.
    // margin + Layer A/B combos + Blend Mode + always-visible Layer Mix +
    // Load Image row + color swatches row
    int totalHeight = 16 + presetRowHeight * 3 + rowHeight + imageRowHeight + colorRowHeight;
    for (auto* groupUI : paramGroupUIs)
        totalHeight += headerHeight + (groupUI->header.expanded ? rowHeight * groupUI->sliders.size() : 0);

    controlsContent.setSize(panelBounds.getWidth() - controlsViewport.getScrollBarThickness(), totalHeight);

    auto content = controlsContent.getLocalBounds().reduced(8, 8);
    auto presetRow = content.removeFromTop(presetRowHeight);
    presetLabel.setBounds(presetRow.removeFromTop(18));
    presetBox.setBounds(presetRow);

    auto layerBRow = content.removeFromTop(presetRowHeight);
    layerBLabel.setBounds(layerBRow.removeFromTop(18));
    layerBBox.setBounds(layerBRow);

    auto blendModeRow = content.removeFromTop(presetRowHeight);
    blendModeLabel.setBounds(blendModeRow.removeFromTop(18));
    blendModeBox.setBounds(blendModeRow);

    auto morphRow = content.removeFromTop(rowHeight);
    layerMixSlider->label.setBounds(morphRow.removeFromTop(18));
    layerMixSlider->slider.setBounds(morphRow);

    auto imageRow = content.removeFromTop(imageRowHeight);
    loadImageButton.setBounds(imageRow.removeFromTop(24));
    imageStatusLabel.setBounds(imageRow);

    auto colorRow = content.removeFromTop(colorRowHeight);
    auto leftColor = colorRow.removeFromLeft(colorRow.getWidth() / 2).reduced(4, 0);
    auto rightColor = colorRow.reduced(4, 0);
    primaryColorLabel.setBounds(leftColor.removeFromTop(18));
    primaryColorSwatch.setBounds(leftColor);
    secondaryColorLabel.setBounds(rightColor.removeFromTop(18));
    secondaryColorSwatch.setBounds(rightColor);

    // Full-bleed section headers read as dividers cutting edge-to-edge
    // across the panel, so they're positioned in the un-reduced content
    // width rather than the 8px-inset column the sliders sit in.
    const int fullWidth = controlsContent.getLocalBounds().getWidth();

    for (auto* groupUI : paramGroupUIs)
    {
        auto headerRow = content.removeFromTop(headerHeight);
        groupUI->header.setBounds(0, headerRow.getY(), fullWidth, headerHeight);

        if (! groupUI->header.expanded)
        {
            for (auto* ps : groupUI->sliders)
            {
                ps->label.setVisible(false);
                ps->slider.setVisible(false);
            }
            continue;
        }

        for (auto* ps : groupUI->sliders)
        {
            ps->label.setVisible(true);
            ps->slider.setVisible(true);
            auto row = content.removeFromTop(rowHeight);
            ps->label.setBounds(row.removeFromTop(18));
            ps->slider.setBounds(row);
        }
    }
}
