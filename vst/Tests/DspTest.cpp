// Offline sanity checks for the TapPanZeit DSP: run an impulse through the
// processor and verify where the taps land. Exits non-zero on failure.
#include "../Source/PluginProcessor.h"

#include <cstdio>

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

void setParam (TapPanZeitProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

// Renders `seconds` of output for a unit impulse at sample 0.
juce::AudioBuffer<float> renderImpulse (TapPanZeitProcessor& p, double sr, double seconds)
{
    constexpr int block = 512;
    const int total = (int) (sr * seconds);
    p.prepareToPlay (sr, block);

    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        if (pos == 0)
        {
            buf.setSample (0, 0, 1.0f);
            buf.setSample (1, 0, 1.0f);
        }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    return out;
}

bool near (float a, float b, float tol = 1.0e-3f) { return std::abs (a - b) <= tol; }
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr double sr = 48000.0;

    {
        TapPanZeitProcessor p;
        setParam (p, ParamIds::taps, 4);
        setParam (p, ParamIds::timeMode, 0);
        setParam (p, ParamIds::timeMs, 100.0f);
        setParam (p, ParamIds::dry, 0.0f);
        setParam (p, ParamIds::wet, 100.0f);
        setParam (p, ParamIds::feedback, 0.0f);
        p.setShape (TapPanZeitProcessor::TimeFn, { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.8f, 0.0f } }); // 100%
        p.setShape (TapPanZeitProcessor::PanFn, { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });  // L -> R
        p.setShape (TapPanZeitProcessor::VolFn, { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });

        auto out = renderImpulse (p, sr, 0.6);
        const int step = 4800; // 100 ms at 48 kHz

        // Pan runs linearly Left -> Right over 4 taps: x = 0, 1/3, 2/3, 1.
        for (int t = 1; t <= 4; ++t)
        {
            const float pan = 1.0f - (float) (t - 1) / 3.0f;
            const int at = step * t;
            char what[128];
            std::snprintf (what, sizeof what, "tap %d at %d ms: L=%.3f R=%.3f (want %.3f / %.3f)", t, 100 * t,
                           out.getSample (0, at), out.getSample (1, at), pan, 1.0f - pan);
            check (near (out.getSample (0, at), pan) && near (out.getSample (1, at), 1.0f - pan), what);
        }

        float stray = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i)
            if (i % step != 0 || i == 0)
                stray = juce::jmax (stray, std::abs (out.getSample (0, i)), std::abs (out.getSample (1, i)));
        check (stray < 1.0e-4f, "no output between taps (dry = 0)");
    }

    {
        // Feedback re-injects the first tap's output: with 1 tap at 100 ms and
        // 50% feedback, repeats decay by half every 100 ms.
        TapPanZeitProcessor p;
        setParam (p, ParamIds::taps, 1);
        setParam (p, ParamIds::timeMode, 0);
        setParam (p, ParamIds::timeMs, 100.0f);
        setParam (p, ParamIds::dry, 0.0f);
        setParam (p, ParamIds::feedback, 50.0f);
        p.setShape (TapPanZeitProcessor::TimeFn, { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.8f, 0.0f } });
        p.setShape (TapPanZeitProcessor::PanFn, { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f } });
        p.setShape (TapPanZeitProcessor::VolFn, { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });

        auto out = renderImpulse (p, sr, 0.45);
        check (near (out.getSample (0, 4800), 0.5f) && near (out.getSample (0, 9600), 0.25f)
                   && near (out.getSample (0, 14400), 0.125f),
               "feedback repeats at 100/200/300 ms decay by 50%");
    }

    {
        // Sync mode: 1/8 at the default 120 BPM (no playhead) = 250 ms per tap.
        TapPanZeitProcessor p;
        setParam (p, ParamIds::taps, 2);
        setParam (p, ParamIds::timeMode, 1);
        setParam (p, ParamIds::timeNote, 6);
        setParam (p, ParamIds::dry, 0.0f);
        p.setShape (TapPanZeitProcessor::TimeFn, { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.8f, 0.0f } });
        p.setShape (TapPanZeitProcessor::PanFn, { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });
        p.setShape (TapPanZeitProcessor::VolFn, { { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } });
        auto out = renderImpulse (p, sr, 0.6);
        check (near (out.getSample (0, 12000), 1.0f) && near (out.getSample (0, 24000), 1.0f),
               "sync 1/8 @ 120 BPM puts taps at 250 and 500 ms");
    }

    {
        // State round-trip keeps shapes, params and presets.
        TapPanZeitProcessor a;
        const Shape custom { { 0.0f, 0.1f, 0.0f }, { 0.3f, 0.9f, 0.4f }, { 1.0f, 0.2f, -0.7f } };
        a.setShape (TapPanZeitProcessor::PanFn, custom);
        setParam (a, ParamIds::taps, 77);
        a.storePreset (3);

        juce::MemoryBlock state;
        a.getStateInformation (state);
        TapPanZeitProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());

        check (b.getShape (TapPanZeitProcessor::PanFn).toString() == custom.toString(), "state restores shapes");
        check ((int) b.apvts.getRawParameterValue (ParamIds::taps)->load() == 77, "state restores params");
        check (b.isPresetUsed (3) && ! b.isPresetUsed (0), "state restores presets");

        setParam (b, ParamIds::taps, 5);
        b.setShape (TapPanZeitProcessor::PanFn, TapPanZeitProcessor::defaultShape (TapPanZeitProcessor::PanFn));
        b.recallPreset (3);
        check ((int) b.apvts.getRawParameterValue (ParamIds::taps)->load() == 77
                   && b.getShape (TapPanZeitProcessor::PanFn).toString() == custom.toString(),
               "preset recall restores params and shapes");
    }

    std::printf ("%s\n", failures == 0 ? "All tests passed" : "Some tests FAILED");
    return failures == 0 ? 0 : 1;
}
