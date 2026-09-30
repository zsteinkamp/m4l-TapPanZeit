#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <vector>

// A breakpoint function, equivalent to Max's [function] object in "curve" mode.
// Both x and y are normalized 0..1. Each point's `curve` shapes the segment that
// ends at that point: 0 = linear, < 0 = fast start (log-ish), > 0 = slow start
// (exp-ish), clamped to -1..1 like Max's `setcurve`.
struct ShapePoint
{
    float x = 0.0f;
    float y = 0.0f;
    float curve = 0.0f;
};

class Shape
{
public:
    std::vector<ShapePoint> points;

    Shape() = default;
    Shape (std::initializer_list<ShapePoint> pts) : points (pts) {}

    static float applyCurve (float t, float c)
    {
        if (std::abs (c) < 1.0e-4f)
            return t;
        const float k = c * curveScale;
        return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
    }

    float eval (float x) const
    {
        if (points.empty())
            return 0.0f;
        if (x <= points.front().x)
            return points.front().y;
        if (x >= points.back().x)
            return points.back().y;

        for (size_t i = 1; i < points.size(); ++i)
        {
            const auto& a = points[i - 1];
            const auto& b = points[i];
            if (x <= b.x)
            {
                const float span = b.x - a.x;
                const float t = span > 0.0f ? (x - a.x) / span : 1.0f;
                return a.y + (b.y - a.y) * applyCurve (t, b.curve);
            }
        }
        return points.back().y;
    }

    juce::String toString() const
    {
        juce::StringArray parts;
        for (const auto& p : points)
            parts.add (juce::String (p.x, 5) + " " + juce::String (p.y, 5) + " " + juce::String (p.curve, 5));
        return parts.joinIntoString (";");
    }

    static Shape fromString (const juce::String& s)
    {
        Shape shape;
        for (const auto& part : juce::StringArray::fromTokens (s, ";", ""))
        {
            auto nums = juce::StringArray::fromTokens (part, " ", "");
            if (nums.size() < 2)
                continue;
            ShapePoint p;
            p.x = juce::jlimit (0.0f, 1.0f, nums[0].getFloatValue());
            p.y = juce::jlimit (0.0f, 1.0f, nums[1].getFloatValue());
            p.curve = nums.size() > 2 ? juce::jlimit (-1.0f, 1.0f, nums[2].getFloatValue()) : 0.0f;
            shape.points.push_back (p);
        }
        std::stable_sort (shape.points.begin(), shape.points.end(),
                          [] (const ShapePoint& a, const ShapePoint& b) { return a.x < b.x; });
        return shape;
    }

private:
    // Approximates the bend of Max's curve mode (setcurve -0.5 lands ~73% of
    // the way through a segment at its midpoint).
    static constexpr float curveScale = 4.0f;
};
