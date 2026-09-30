#include "FunctionEditor.h"

namespace
{
constexpr float pointRadius = 5.0f;
constexpr float hitRadius = 8.0f;
constexpr float labelWidth = 62.0f;

juce::Path shapePath (const Shape& shape, juce::Rectangle<float> r, int steps)
{
    juce::Path path;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const juce::Point<float> pt { r.getX() + x * r.getWidth(), r.getBottom() - shape.eval (x) * r.getHeight() };
        if (i == 0)
            path.startNewSubPath (pt);
        else
            path.lineTo (pt);
    }
    return path;
}
} // namespace

//==============================================================================
void ShapePresetButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f, 4.0f);
    const auto colour = down ? Colours::accent : highlighted ? Colours::text : Colours::text.withAlpha (0.55f);
    g.setColour (colour);
    g.strokePath (shapePath (shape, r, 40), juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                                  juce::PathStrokeType::rounded));
}

//==============================================================================
FunctionPanel::FunctionPanel (TapPanZeitProcessor& p, int fnIndex, juce::String t, std::vector<AxisLabel> l,
                              std::vector<Shape> presetShapes, const float* meterLevels)
    : processor (p), fn (fnIndex), title (std::move (t)), labels (std::move (l)), meters (meterLevels)
{
    for (auto& s : presetShapes)
    {
        auto* b = presetButtons.add (new ShapePresetButton (s));
        b->setTooltip ("Apply preset shape");
        b->onClick = [this, b] {
            shape = b->shape;
            commit();
        };
        addAndMakeVisible (b);
    }
    reload();
}

void FunctionPanel::reload()
{
    shape = processor.getShape (fn);
    repaint();
}

void FunctionPanel::resized()
{
    auto bounds = getLocalBounds().toFloat();
    plot = bounds.withTrimmedLeft (labelWidth).withTrimmedTop (32.0f).withTrimmedRight (14.0f).withTrimmedBottom (12.0f);

    auto buttonArea = getLocalBounds().removeFromTop (28).withTrimmedRight (10);
    for (int i = presetButtons.size(); --i >= 0;)
        presetButtons[i]->setBounds (buttonArea.removeFromRight (32).withSizeKeepingCentre (30, 20));
}

juce::Point<float> FunctionPanel::toScreen (const ShapePoint& p) const
{
    return { plot.getX() + p.x * plot.getWidth(), plot.getBottom() - p.y * plot.getHeight() };
}

ShapePoint FunctionPanel::fromScreen (juce::Point<float> pos) const
{
    ShapePoint p;
    p.x = juce::jlimit (0.0f, 1.0f, (pos.x - plot.getX()) / plot.getWidth());
    p.y = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - pos.y) / plot.getHeight());
    return p;
}

int FunctionPanel::hitPoint (juce::Point<float> pos) const
{
    int best = -1;
    float bestDist = hitRadius;
    for (int i = 0; i < (int) shape.points.size(); ++i)
    {
        const float d = toScreen (shape.points[(size_t) i]).getDistanceFrom (pos);
        if (d <= bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

int FunctionPanel::segmentAt (float screenX) const
{
    const float x = juce::jlimit (0.0f, 1.0f, (screenX - plot.getX()) / plot.getWidth());
    for (int i = 1; i < (int) shape.points.size(); ++i)
        if (x <= shape.points[(size_t) i].x)
            return i;
    return (int) shape.points.size() - 1;
}

void FunctionPanel::commit()
{
    processor.setShape (fn, shape);
    repaint();
}

//==============================================================================
void FunctionPanel::mouseDown (const juce::MouseEvent& e)
{
    drag = Drag::None;
    const auto pos = e.position;
    if (! plot.expanded (hitRadius).contains (pos))
        return;

    int idx = hitPoint (pos);
    const int last = (int) shape.points.size() - 1;

    if (e.mods.isShiftDown())
    {
        if (idx > 0 && idx < last)
        {
            shape.points.erase (shape.points.begin() + idx);
            commit();
        }
        return;
    }

    if (e.mods.isAltDown())
    {
        dragIndex = segmentAt (pos.x);
        if (dragIndex > 0)
        {
            drag = Drag::Curve;
            dragStartCurve = shape.points[(size_t) dragIndex].curve;
        }
        return;
    }

    if (idx < 0)
    {
        auto np = fromScreen (pos);
        idx = segmentAt (pos.x);
        np.x = juce::jlimit (shape.points[(size_t) idx - 1].x, shape.points[(size_t) idx].x, np.x);
        shape.points.insert (shape.points.begin() + idx, np);
        commit();
    }

    drag = Drag::Point;
    dragIndex = idx;
}

void FunctionPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::Point && dragIndex >= 0)
    {
        auto np = fromScreen (e.position);
        auto& pt = shape.points[(size_t) dragIndex];
        const int last = (int) shape.points.size() - 1;
        if (dragIndex == 0)
            np.x = 0.0f;
        else if (dragIndex == last)
            np.x = 1.0f;
        else
            np.x = juce::jlimit (shape.points[(size_t) dragIndex - 1].x, shape.points[(size_t) dragIndex + 1].x, np.x);
        pt.x = np.x;
        pt.y = np.y;
        commit();
    }
    else if (drag == Drag::Curve && dragIndex > 0)
    {
        const auto& a = shape.points[(size_t) dragIndex - 1];
        auto& b = shape.points[(size_t) dragIndex];
        const float up = -(float) e.getDistanceFromDragStartY() / 100.0f;
        const float sign = b.y >= a.y ? 1.0f : -1.0f;
        b.curve = juce::jlimit (-1.0f, 1.0f, dragStartCurve - up * sign);
        commit();
    }
}

void FunctionPanel::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::None;
    dragIndex = -1;
}

void FunctionPanel::mouseMove (const juce::MouseEvent& e)
{
    const int idx = hitPoint (e.position);
    if (idx != hoverIndex)
    {
        hoverIndex = idx;
        repaint();
    }
}

//==============================================================================
void FunctionPanel::paintMeters (juce::Graphics& g, int numTaps)
{
    const float step = numTaps > 1 ? plot.getWidth() / (float) (numTaps - 1) : plot.getWidth();
    const float barW = juce::jlimit (1.5f, 10.0f, step * 0.8f);
    const float centreY = plot.getCentreY();

    for (int i = 0; i < numTaps; ++i)
    {
        const float x = numTaps > 1 ? (float) i / (float) (numTaps - 1) : 0.0f;
        const float sx = plot.getX() + x * plot.getWidth();
        const float level = std::sqrt (juce::jlimit (0.0f, 1.0f, meters[i]));

        // Faint column marker for every tap.
        g.setColour (Colours::grid.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (sx), plot.getY(), plot.getBottom());

        if (fn == TapPanZeitProcessor::PanFn)
        {
            const float panY = plot.getBottom() - shape.eval (x) * plot.getHeight();
            const float endY = centreY + (panY - centreY) * level;
            g.setColour (Colours::tealDark);
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (sx - barW * 0.5f, juce::jmin (centreY, endY),
                                                                   sx + barW * 0.5f, juce::jmax (centreY, endY)));
            g.setColour (Colours::teal.withAlpha (0.35f + 0.65f * level));
            g.fillRect (sx - barW * 0.5f, panY - 1.0f, barW, 2.0f);
        }
        else
        {
            const float h = level * plot.getHeight();
            g.setColour (Colours::tealDark);
            g.fillRect (sx - barW * 0.5f, plot.getBottom() - h, barW, h);
            g.setColour (Colours::teal);
            g.fillRect (sx - barW * 0.5f, plot.getBottom() - h - 1.0f, barW, 2.0f);
        }
    }
}

void FunctionPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (title, bounds.withTrimmedLeft (labelWidth + 8.0f).removeFromTop (30.0f), juce::Justification::centredLeft);

    // Horizontal guides + axis labels.
    g.setFont (juce::FontOptions (13.0f));
    for (const auto& l : labels)
    {
        const float y = plot.getBottom() - l.y * plot.getHeight();
        g.setColour (Colours::grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (Colours::accent);
        g.drawText (l.text, juce::Rectangle<float> (0.0f, y - 8.0f, labelWidth - 6.0f, 16.0f),
                    juce::Justification::centredRight);
    }

    const int numTaps = (int) processor.apvts.getRawParameterValue (ParamIds::taps)->load();
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.expanded (1.0f).toNearestInt());
        paintMeters (g, numTaps);
    }

    g.setColour (Colours::accent);
    g.strokePath (shapePath (shape, plot, juce::jmax (64, (int) plot.getWidth())),
                  juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int i = 0; i < (int) shape.points.size(); ++i)
    {
        const auto c = toScreen (shape.points[(size_t) i]);
        const float r = (i == hoverIndex || i == dragIndex) ? pointRadius + 1.5f : pointRadius;
        g.setColour (Colours::accent);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
}
