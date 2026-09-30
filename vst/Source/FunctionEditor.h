#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Shape.h"

namespace Colours
{
inline const juce::Colour background { 0xff2a2a2a };
inline const juce::Colour panel { 0xff161616 };
inline const juce::Colour grid { 0xff333333 };
inline const juce::Colour text { 0xffd8d8d8 };
inline const juce::Colour accent { 0xfff5a55a };  // orange
inline const juce::Colour teal { 0xff1fc6d8 };
inline const juce::Colour tealDark { 0xff0d4f52 };
} // namespace Colours

// A clickable icon that applies a preset shape to a FunctionPanel.
class ShapePresetButton : public juce::Button
{
public:
    ShapePresetButton (const Shape& s) : juce::Button ("shape"), shape (s) {}
    Shape shape;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
};

// Drawable breakpoint function, like Max's [function]:
//   click to add / drag points, shift-click to remove, alt-drag to bend a segment.
class FunctionPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct AxisLabel
    {
        float y;
        juce::String text;
    };

    FunctionPanel (TapPanZeitProcessor& p, int fn, juce::String title, std::vector<AxisLabel> labels,
                   std::vector<Shape> presetShapes, const float* meterLevels);

    void reload();
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::Point<float> toScreen (const ShapePoint& p) const;
    ShapePoint fromScreen (juce::Point<float> pos) const;
    int hitPoint (juce::Point<float> pos) const;
    int segmentAt (float screenX) const;
    void commit();
    void paintMeters (juce::Graphics&, int numTaps);

    TapPanZeitProcessor& processor;
    const int fn;
    const juce::String title;
    const std::vector<AxisLabel> labels;
    const float* meters;

    Shape shape;
    juce::Rectangle<float> plot;
    juce::OwnedArray<ShapePresetButton> presetButtons;

    enum class Drag
    {
        None,
        Point,
        Curve
    } drag = Drag::None;
    int dragIndex = -1;
    float dragStartCurve = 0.0f;
    int hoverIndex = -1;
};
