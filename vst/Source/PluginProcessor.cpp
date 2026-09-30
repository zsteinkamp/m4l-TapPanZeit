#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* const shapePropNames[] = { "timeFn", "panFn", "volFn" };

juce::ParameterID pid (const char* id) { return juce::ParameterID { id, 1 }; }

juce::String formatPercent (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " %"; }
juce::String formatMs (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
}
} // namespace

//==============================================================================
TapPanZeitProcessor::TapPanZeitProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TapPanZeit", createLayout())
{
    for (int fn = 0; fn < NumFns; ++fn)
        setShape (fn, defaultShape (fn));
}

const juce::StringArray& TapPanZeitProcessor::noteNames()
{
    static const juce::StringArray names { "1/64", "1/48", "1/32", "1/24", "1/16", "1/12", "1/8", "1/6",
                                           "3/16", "1/4",  "5/16", "1/3",  "3/8",  "1/2",  "3/4", "1",
                                           "1.5",  "2",    "3",    "4",    "6",    "8",    "16",  "32" };
    return names;
}

// Note values as a fraction of a whole note (4 beats), same order as noteNames().
const std::vector<double>& TapPanZeitProcessor::noteWholeFractions()
{
    static const std::vector<double> fractions { 1.0 / 64, 1.0 / 48, 1.0 / 32, 1.0 / 24, 1.0 / 16, 1.0 / 12,
                                                 1.0 / 8,  1.0 / 6,  3.0 / 16, 1.0 / 4,  5.0 / 16, 1.0 / 3,
                                                 3.0 / 8,  1.0 / 2,  3.0 / 4,  1.0,      1.5,      2.0,
                                                 3.0,      4.0,      6.0,      8.0,      16.0,     32.0 };
    return fractions;
}

juce::AudioProcessorValueTreeState::ParameterLayout TapPanZeitProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (pid (ParamIds::taps), "Taps", 1, maxTaps, 32));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIds::timeMode), "Time Mode",
                                                        StringArray { "Free", "Sync" }, 1));

    // Max's live.dial exponent 3 == JUCE skew 1/3.
    NormalisableRange<float> msRange (0.0f, 8000.0f, 0.0f, 1.0f / 3.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        pid (ParamIds::timeMs), "Time Base", msRange, 200.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (formatMs)));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIds::timeNote), "Time Note", noteNames(), 6));

    auto pct = [] (const char* id, const char* name, float def) {
        return std::make_unique<AudioParameterFloat> (
            pid (id), name, NormalisableRange<float> (0.0f, 100.0f), def,
            AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (formatPercent));
    };
    layout.add (pct (ParamIds::dry, "Dry Vol", 100.0f));
    layout.add (pct (ParamIds::wet, "Wet Vol", 100.0f));
    layout.add (pct (ParamIds::feedback, "Feedback", 0.0f));

    return layout;
}

//==============================================================================
Shape TapPanZeitProcessor::defaultShape (int fn)
{
    switch (fn)
    {
        case TimeFn: return { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.02f, -0.5f } };
        case PanFn:
            return { { 0.0f, 1.0f, 0.0f },
                     { 0.25f, 0.0f, 0.5f },
                     { 0.5f, 0.5f, -0.5f },
                     { 0.75f, 0.0f, 0.5f },
                     { 1.0f, 0.75f, -0.5f } };
        case VolFn:
        default: return { { 0.0f, 1.0f, 0.0f }, { 0.25f, 0.0f, 0.5f }, { 1.0f, 0.8f, -0.5f } };
    }
}

Shape TapPanZeitProcessor::getShape (int fn) const
{
    const juce::ScopedLock sl (shapeLock);
    return shapes[(size_t) fn];
}

void TapPanZeitProcessor::setShape (int fn, const Shape& shape)
{
    const juce::ScopedLock sl (shapeLock);
    shapes[(size_t) fn] = shape.points.size() >= 2 ? shape : defaultShape (fn);
    rebuildTable (fn);
}

void TapPanZeitProcessor::rebuildTable (int fn)
{
    auto& table = tables[(size_t) fn];
    const auto& shape = shapes[(size_t) fn];
    for (int i = 0; i <= tableSize; ++i)
        table[(size_t) i].store (shape.eval ((float) i / (float) tableSize), std::memory_order_relaxed);
}

float TapPanZeitProcessor::lookup (int fn, float x) const
{
    const auto& table = tables[(size_t) fn];
    const float pos = juce::jlimit (0.0f, 1.0f, x) * (float) tableSize;
    const int i = juce::jmin ((int) pos, tableSize - 1);
    const float frac = pos - (float) i;
    const float a = table[(size_t) i].load (std::memory_order_relaxed);
    const float b = table[(size_t) i + 1].load (std::memory_order_relaxed);
    return a + (b - a) * frac;
}

//==============================================================================
void TapPanZeitProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    bufferLength = (int) std::ceil (maxTotalDelaySeconds * sr) + 4;
    delayBuffer.setSize (2, bufferLength);
    delayBuffer.clear();
    writePos = 0;
    lastActiveTaps = 0;

    delaySmoothCoeff = 1.0f - (float) std::exp (-1.0 / (0.05 * sr));
    gainSmoothCoeff = 1.0f - (float) std::exp (-1.0 / (0.01 * sr));

    for (auto* s : { &drySmoothed, &wetSmoothed, &feedbackSmoothed })
        s->reset (sr, 0.02);
    drySmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIds::dry)->load() * 0.01f);
    wetSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIds::wet)->load() * 0.01f);
    feedbackSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIds::feedback)->load() * 0.01f);
}

void TapPanZeitProcessor::releaseResources()
{
    delayBuffer.setSize (0, 0);
    bufferLength = 0;
}

bool TapPanZeitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

double TapPanZeitProcessor::currentTimeBaseMs()
{
    if (apvts.getRawParameterValue (ParamIds::timeMode)->load() < 0.5f)
        return apvts.getRawParameterValue (ParamIds::timeMs)->load();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                if (*hostBpm > 0.0)
                    bpm = *hostBpm;

    const auto& fractions = noteWholeFractions();
    const int idx = juce::jlimit (0, (int) fractions.size() - 1,
                                  (int) apvts.getRawParameterValue (ParamIds::timeNote)->load());
    return fractions[(size_t) idx] * 4.0 * 60000.0 / bpm;
}

void TapPanZeitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (bufferLength == 0 || buffer.getNumChannels() < 2 || numIn < 1)
        return;

    // --- Per-tap targets from the three functions ---
    // Taps are chained in the original (tap N delays tap N-1's output), so tap N
    // hears the input delayed by the sum of every tap's delay up to and including N.
    const int numTaps = juce::jlimit (1, maxTaps, (int) apvts.getRawParameterValue (ParamIds::taps)->load());
    const double samplesPerMs = currentTimeBaseMs() * sr / 1000.0;
    const float maxTapSamples = (float) (maxTapDelaySeconds * sr);
    const float maxTotalSamples = (float) (bufferLength - 2);

    float cumulative = 0.0f;
    for (int i = 0; i < numTaps; ++i)
    {
        const float x = numTaps > 1 ? (float) i / (float) (numTaps - 1) : 0.0f;
        // Whole samples, like Max's [delay~].
        const float d = juce::jlimit (0.0f, maxTapSamples,
                                      std::round ((float) (samplesPerMs * lookup (TimeFn, x) * timeFnScale)));
        cumulative = juce::jmin (cumulative + d, maxTotalSamples);
        targetDelay[(size_t) i] = cumulative;

        // Pan 1 = Left, 0 = Right (linear, as in the M4L device).
        const float pan = lookup (PanFn, x);
        const float vol = lookup (VolFn, x);
        targetGainL[(size_t) i] = vol * pan;
        targetGainR[(size_t) i] = vol * (1.0f - pan);

        if (i >= lastActiveTaps)
        {
            curDelay[(size_t) i] = cumulative;
            curGainL[(size_t) i] = 0.0f;
            curGainR[(size_t) i] = 0.0f;
        }
    }
    // Taps that were just switched off fade out rather than click.
    for (int i = numTaps; i < lastActiveTaps; ++i)
    {
        targetGainL[(size_t) i] = 0.0f;
        targetGainR[(size_t) i] = 0.0f;
    }
    const int active = juce::jmax (numTaps, lastActiveTaps);

    drySmoothed.setTargetValue (apvts.getRawParameterValue (ParamIds::dry)->load() * 0.01f);
    wetSmoothed.setTargetValue (apvts.getRawParameterValue (ParamIds::wet)->load() * 0.01f);
    feedbackSmoothed.setTargetValue (apvts.getRawParameterValue (ParamIds::feedback)->load() * 0.01f);

    float* bufL = delayBuffer.getWritePointer (0);
    float* bufR = delayBuffer.getWritePointer (1);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    const int len = bufferLength;
    auto readAt = [len] (const float* b, int wp, float delay) {
        const int di = (int) delay;
        const float frac = delay - (float) di;
        int i0 = wp - di;
        if (i0 < 0)
            i0 += len;
        int i1 = i0 - 1;
        if (i1 < 0)
            i1 += len;
        return b[i0] + frac * (b[i1] - b[i0]);
    };

    std::array<float, maxTaps> peakPre {}, peakPost {};

    for (int s = 0; s < numSamples; ++s)
    {
        const float dry = drySmoothed.getNextValue();
        const float wet = wetSmoothed.getNextValue();
        const float fb = feedbackSmoothed.getNextValue();
        const float xl = inL[s];
        const float xr = inR[s];

        // Feedback comes from the output of the first tap, back into its input.
        const float fbDelay = juce::jmax (1.0f, curDelay[0]);
        const float fbl = readAt (bufL, writePos, fbDelay);
        const float fbr = readAt (bufR, writePos, fbDelay);
        bufL[writePos] = xl + fb * fbl;
        bufR[writePos] = xr + fb * fbr;

        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < active; ++i)
        {
            const auto t = (size_t) i;
            // Glide toward a new delay time, then snap so settled taps read exact samples.
            const float delayDiff = targetDelay[t] - curDelay[t];
            curDelay[t] = std::abs (delayDiff) < 1.0e-3f ? targetDelay[t] : curDelay[t] + delayDiff * delaySmoothCoeff;
            curGainL[t] += (targetGainL[t] - curGainL[t]) * gainSmoothCoeff;
            curGainR[t] += (targetGainR[t] - curGainR[t]) * gainSmoothCoeff;

            const float dl = readAt (bufL, writePos, curDelay[t]);
            const float dr = readAt (bufR, writePos, curDelay[t]);
            const float ol = dl * curGainL[t];
            const float orr = dr * curGainR[t];
            wl += ol;
            wr += orr;

            peakPre[t] = juce::jmax (peakPre[t], 0.5f * (std::abs (dl) + std::abs (dr)));
            peakPost[t] = juce::jmax (peakPost[t], 0.5f * (std::abs (ol) + std::abs (orr)));
        }

        outL[s] = xl * dry + wl * wet;
        outR[s] = xr * dry + wr * wet;

        if (++writePos >= len)
            writePos = 0;
    }

    for (int i = 0; i < active; ++i)
    {
        const auto t = (size_t) i;
        if (peakPre[t] > meterPre[t].load (std::memory_order_relaxed))
            meterPre[t].store (peakPre[t], std::memory_order_relaxed);
        if (peakPost[t] > meterPost[t].load (std::memory_order_relaxed))
            meterPost[t].store (peakPost[t], std::memory_order_relaxed);
    }

    // Keep fading taps alive until they are silent.
    int stillActive = numTaps;
    for (int i = numTaps; i < lastActiveTaps; ++i)
        if (std::abs (curGainL[(size_t) i]) > 1.0e-5f || std::abs (curGainR[(size_t) i]) > 1.0e-5f)
            stillActive = i + 1;
    lastActiveTaps = stillActive;
}

//==============================================================================
bool TapPanZeitProcessor::isPresetUsed (int slot) const
{
    return presets.getChildWithProperty ("slot", slot).isValid();
}

void TapPanZeitProcessor::storePreset (int slot)
{
    juce::ValueTree p ("Preset");
    p.setProperty ("slot", slot, nullptr);
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            p.setProperty (ranged->getParameterID(), ranged->getValue(), nullptr);
    for (int fn = 0; fn < NumFns; ++fn)
        p.setProperty (shapePropNames[fn], getShape (fn).toString(), nullptr);

    clearPreset (slot);
    presets.appendChild (p, nullptr);
    currentPreset = slot;
    ++presetsVersion;
}

void TapPanZeitProcessor::recallPreset (int slot)
{
    auto p = presets.getChildWithProperty ("slot", slot);
    if (! p.isValid())
        return;

    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (p.hasProperty (ranged->getParameterID()))
            {
                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost ((float) p.getProperty (ranged->getParameterID()));
                ranged->endChangeGesture();
            }
    for (int fn = 0; fn < NumFns; ++fn)
        if (p.hasProperty (shapePropNames[fn]))
            setShape (fn, Shape::fromString (p.getProperty (shapePropNames[fn]).toString()));

    currentPreset = slot;
    ++shapesVersion;
    ++presetsVersion;
}

void TapPanZeitProcessor::clearPreset (int slot)
{
    auto existing = presets.getChildWithProperty ("slot", slot);
    if (existing.isValid())
        presets.removeChild (existing, nullptr);
    ++presetsVersion;
}

void TapPanZeitProcessor::clearAllPresets()
{
    presets.removeAllChildren (nullptr);
    currentPreset = 0;
    ++presetsVersion;
}

//==============================================================================
void TapPanZeitProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::ValueTree shapesTree ("Shapes");
    for (int fn = 0; fn < NumFns; ++fn)
        shapesTree.setProperty (shapePropNames[fn], getShape (fn).toString(), nullptr);

    state.removeChild (state.getChildWithName ("Shapes"), nullptr);
    state.removeChild (state.getChildWithName ("Presets"), nullptr);
    state.appendChild (shapesTree, nullptr);
    state.appendChild (presets.createCopy(), nullptr);
    state.setProperty ("currentPreset", currentPreset, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TapPanZeitProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    auto shapesTree = state.getChildWithName ("Shapes");
    for (int fn = 0; fn < NumFns; ++fn)
        setShape (fn, shapesTree.hasProperty (shapePropNames[fn])
                          ? Shape::fromString (shapesTree.getProperty (shapePropNames[fn]).toString())
                          : defaultShape (fn));

    auto presetsTree = state.getChildWithName ("Presets");
    presets = presetsTree.isValid() ? presetsTree.createCopy() : juce::ValueTree ("Presets");
    currentPreset = state.getProperty ("currentPreset", 0);

    state.removeChild (shapesTree, nullptr);
    state.removeChild (presetsTree, nullptr);
    apvts.replaceState (state);

    ++shapesVersion;
    ++presetsVersion;
}

//==============================================================================
juce::AudioProcessorEditor* TapPanZeitProcessor::createEditor() { return new TapPanZeitEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TapPanZeitProcessor(); }
