#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1000;
constexpr int editorHeight = 200;
} // namespace

//==============================================================================
TapPanZeitLookAndFeel::TapPanZeitLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::panel);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panel);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
}

void TapPanZeitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float lineW = 3.5f;
    const float arcR = radius - lineW * 0.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::grid);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    if (pos > 0.0f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (Colours::teal);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }

    const juce::Point<float> tip = centre.getPointOnCircumference (arcR, angle);
    g.setColour (Colours::text);
    g.drawLine ({ centre, tip }, 2.0f);
}

void TapPanZeitLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                                  bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? Colours::accent : Colours::panel);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour ((highlighted || down) ? Colours::accent : Colours::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
}

//==============================================================================
juce::Rectangle<float> PresetGrid::cellBounds (int slot) const
{
    const float cell = juce::jmin ((float) getWidth(), (float) getHeight()) / 4.0f;
    const float ox = ((float) getWidth() - cell * 4.0f) * 0.5f;
    return { ox + (float) (slot % 4) * cell, (float) (slot / 4) * cell, cell, cell };
}

void PresetGrid::paint (juce::Graphics& g)
{
    g.setFont (juce::FontOptions (11.0f));
    for (int slot = 0; slot < TapPanZeitProcessor::numPresets; ++slot)
    {
        const auto r = cellBounds (slot).reduced (2.5f);
        const bool used = proc.isPresetUsed (slot);
        const bool current = used && proc.getCurrentPreset() == slot;
        g.setColour (current ? Colours::accent : used ? Colours::text : Colours::grid);
        g.fillRect (r);
        if (used)
        {
            g.setColour (juce::Colours::black);
            g.drawText (juce::String (slot + 1), r, juce::Justification::centred);
        }
    }
}

void PresetGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int slot = 0; slot < TapPanZeitProcessor::numPresets; ++slot)
    {
        if (! cellBounds (slot).contains (e.position))
            continue;
        if (e.mods.isShiftDown())
            proc.storePreset (slot);
        else if (e.mods.isAltDown())
            proc.clearPreset (slot);
        else
            proc.recallPreset (slot);
        repaint();
        return;
    }
}

//==============================================================================
TapPanZeitEditor::TapPanZeitEditor (TapPanZeitProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      timePanel (p, TapPanZeitProcessor::TimeFn, "Time", { { 0.8f, "100%" }, { 0.4f, "50%" }, { 0.0f, "0%" } },
                 { Shape { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.8f, 0.0f } },
                   Shape { { 0.0f, 0.02f, 0.0f }, { 1.0f, 0.8f, 0.5f } },
                   Shape { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.02f, -0.5f } } },
                 displayPre.data()),
      panPanel (p, TapPanZeitProcessor::PanFn, "Pan", { { 1.0f, "Left" }, { 0.5f, "Center" }, { 0.0f, "Right" } },
                { Shape { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f } },
                  TapPanZeitProcessor::defaultShape (TapPanZeitProcessor::PanFn),
                  Shape { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.5f } } },
                displayPre.data()),
      volPanel (p, TapPanZeitProcessor::VolFn, "Volume", { { 1.0f, "100%" }, { 0.5f, "50%" }, { 0.0f, "0%" } },
                { Shape { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } },
                  TapPanZeitProcessor::defaultShape (TapPanZeitProcessor::VolFn),
                  Shape { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, -0.5f } } },
                displayPost.data()),
      presetGrid (p)
{
    setLookAndFeel (&lnf);

    setupKnob (tapsKnob, tapsLabel, "Taps");
    setupKnob (timeMsKnob, timeLabel, "Time Base");
    setupKnob (timeNoteKnob, timeLabel, "Time Base");
    setupKnob (dryKnob, dryLabel, "Dry Vol");
    setupKnob (wetKnob, wetLabel, "Wet Vol");
    setupKnob (feedbackKnob, feedbackLabel, "Feedback");

    auto& apvts = proc.apvts;
    tapsAtt = std::make_unique<SliderAttachment> (apvts, ParamIds::taps, tapsKnob);
    timeMsAtt = std::make_unique<SliderAttachment> (apvts, ParamIds::timeMs, timeMsKnob);
    timeNoteAtt = std::make_unique<SliderAttachment> (apvts, ParamIds::timeNote, timeNoteKnob);
    dryAtt = std::make_unique<SliderAttachment> (apvts, ParamIds::dry, dryKnob);
    wetAtt = std::make_unique<SliderAttachment> (apvts, ParamIds::wet, wetKnob);
    feedbackAtt = std::make_unique<SliderAttachment> (apvts, ParamIds::feedback, feedbackKnob);

    for (auto* b : { &freeButton, &syncButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    freeButton.setTooltip ("Free time (ms)");
    syncButton.setTooltip ("Tempo-synced time");
    modeAtt = std::make_unique<juce::ParameterAttachment> (
        *apvts.getParameter (ParamIds::timeMode), [this] (float v) { setTimeMode (juce::roundToInt (v)); });
    freeButton.onClick = [this] { modeAtt->setValueAsCompleteGesture (0.0f); };
    syncButton.onClick = [this] { modeAtt->setValueAsCompleteGesture (1.0f); };
    modeAtt->sendInitialUpdate();

    addAndMakeVisible (timePanel);
    addAndMakeVisible (panPanel);
    addAndMakeVisible (volPanel);
    for (auto* panel : { &timePanel, &panPanel, &volPanel })
        panel->setTooltip ("Click to add points. Drag to move. Shift-click to remove. Alt-drag to curve.");

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setColour (juce::Label::textColourId, Colours::accent);
    presetLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (presetLabel);
    presetHint.setText ("Shift-click: store\nAlt-click: clear", juce::dontSendNotification);
    presetHint.setFont (juce::FontOptions (10.5f));
    presetHint.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (presetHint);
    addAndMakeVisible (presetGrid);
    clearAllButton.onClick = [this] {
        proc.clearAllPresets();
        presetGrid.repaint();
    };
    addAndMakeVisible (clearAllButton);
    aboutButton.onClick = [] { juce::URL ("https://plugins.steinkamp.us/m4l-TapPanZeit").launchInDefaultBrowser(); };
    addAndMakeVisible (aboutButton);

    setSize (editorWidth, editorHeight);
    startTimerHz (30);
}

TapPanZeitEditor::~TapPanZeitEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TapPanZeitEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setFont (juce::FontOptions (13.5f));
    addAndMakeVisible (l);
}

void TapPanZeitEditor::setTimeMode (int mode)
{
    timeMode = mode;
    freeButton.setToggleState (mode == 0, juce::dontSendNotification);
    syncButton.setToggleState (mode == 1, juce::dontSendNotification);
    timeMsKnob.setVisible (mode == 0);
    timeNoteKnob.setVisible (mode == 1);
}

void TapPanZeitEditor::paint (juce::Graphics& g) { g.fillAll (Colours::background); }

void TapPanZeitEditor::resized()
{
    auto area = getLocalBounds().reduced (6);

    // Left column: Taps, Time Base, mode.
    auto left = area.removeFromLeft (80);
    tapsLabel.setBounds (left.removeFromTop (16));
    tapsKnob.setBounds (left.removeFromTop (62));
    left.removeFromTop (4);
    timeLabel.setBounds (left.removeFromTop (16));
    timeMsKnob.setBounds (left.removeFromTop (62));
    timeNoteKnob.setBounds (timeMsKnob.getBounds());
    auto modeRow = left.removeFromTop (22).reduced (2, 1);
    freeButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2).withTrimmedRight (2));
    syncButton.setBounds (modeRow.withTrimmedLeft (2));

    // Right: preset panel, then mix knobs.
    auto presets = area.removeFromRight (108);
    area.removeFromRight (6);
    auto mix = area.removeFromRight (72);
    area.removeFromRight (6);

    const int knobH = mix.getHeight() / 3;
    for (auto [knob, label] : { std::pair { &dryKnob, &dryLabel }, std::pair { &wetKnob, &wetLabel },
                                std::pair { &feedbackKnob, &feedbackLabel } })
    {
        auto cell = mix.removeFromTop (knobH);
        label->setBounds (cell.removeFromTop (14));
        knob->setBounds (cell);
    }

    presetLabel.setBounds (presets.removeFromTop (18));
    aboutButton.setBounds (presets.removeFromBottom (18).reduced (8, 0));
    presets.removeFromBottom (4);
    clearAllButton.setBounds (presets.removeFromBottom (20).reduced (8, 0));
    presetHint.setBounds (presets.removeFromBottom (28));
    presetGrid.setBounds (presets);

    // Three function panels share the middle.
    area.removeFromLeft (6);
    const int gap = 6;
    const int panelW = (area.getWidth() - gap * 2) / 3;
    timePanel.setBounds (area.removeFromLeft (panelW));
    area.removeFromLeft (gap);
    panPanel.setBounds (area.removeFromLeft (panelW));
    area.removeFromLeft (gap);
    volPanel.setBounds (area);
}

void TapPanZeitEditor::timerCallback()
{
    constexpr float decay = 0.85f;
    for (size_t i = 0; i < displayPre.size(); ++i)
    {
        displayPre[i] = juce::jmax (proc.meterPre[i].exchange (0.0f), displayPre[i] * decay);
        displayPost[i] = juce::jmax (proc.meterPost[i].exchange (0.0f), displayPost[i] * decay);
    }

    if (const int v = proc.shapesVersion.load(); v != lastShapesVersion)
    {
        lastShapesVersion = v;
        timePanel.reload();
        panPanel.reload();
        volPanel.reload();
    }
    if (const int v = proc.presetsVersion.load(); v != lastPresetsVersion)
    {
        lastPresetsVersion = v;
        presetGrid.repaint();
    }

    timePanel.repaint();
    panPanel.repaint();
    volPanel.repaint();
}
