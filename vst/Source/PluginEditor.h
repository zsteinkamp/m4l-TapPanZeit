#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FunctionEditor.h"
#include "PluginProcessor.h"

class TapPanZeitLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TapPanZeitLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted,
                               bool down) override;
};

// 4x4 grid of preset slots: click to recall, shift-click to store, alt-click to clear.
class PresetGrid : public juce::Component
{
public:
    explicit PresetGrid (TapPanZeitProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cellBounds (int slot) const;
    TapPanZeitProcessor& proc;
};

class TapPanZeitEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TapPanZeitEditor (TapPanZeitProcessor&);
    ~TapPanZeitEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& text);
    void setTimeMode (int mode);

    TapPanZeitProcessor& proc;
    TapPanZeitLookAndFeel lnf;
    juce::TooltipWindow tooltips { this };

    std::array<float, TapPanZeitProcessor::maxTaps> displayPre {}, displayPost {};

    juce::Slider tapsKnob, timeMsKnob, timeNoteKnob, dryKnob, wetKnob, feedbackKnob;
    juce::Label tapsLabel, timeLabel, dryLabel, wetLabel, feedbackLabel;
    juce::TextButton freeButton { "ms" }, syncButton { "Sync" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> tapsAtt, timeMsAtt, timeNoteAtt, dryAtt, wetAtt, feedbackAtt;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    int timeMode = 1;

    FunctionPanel timePanel, panPanel, volPanel;

    juce::Label presetLabel, presetHint;
    PresetGrid presetGrid;
    juce::TextButton clearAllButton { "Clear All" }, aboutButton { "About" };

    int lastShapesVersion = -1, lastPresetsVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapPanZeitEditor)
};
