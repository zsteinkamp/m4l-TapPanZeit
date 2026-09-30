#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Shape.h"

#include <array>
#include <atomic>
#include <vector>

namespace ParamIds
{
inline constexpr const char* taps = "taps";
inline constexpr const char* timeMode = "timeMode";
inline constexpr const char* timeMs = "timeMs";
inline constexpr const char* timeNote = "timeNote";
inline constexpr const char* dry = "dry";
inline constexpr const char* wet = "wet";
inline constexpr const char* feedback = "feedback";
} // namespace ParamIds

class TapPanZeitProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxTaps = 128;
    static constexpr int numPresets = 16;
    // The time function's 0..1 range maps to 0..125% of the time base.
    static constexpr float timeFnScale = 1.25f;
    // Max's [delay~ 441000] inside each poly~ voice: ~10 s per tap.
    static constexpr double maxTapDelaySeconds = 10.0;
    // Total (cumulative) delay across all taps. The M4L device chains 128
    // separate 10 s delay lines; one shared buffer is far cheaper but needs a cap.
    static constexpr double maxTotalDelaySeconds = 120.0;

    enum Fn
    {
        TimeFn = 0,
        PanFn,
        VolFn,
        NumFns
    };

    TapPanZeitProcessor();
    ~TapPanZeitProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return maxTotalDelaySeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // --- Shapes (message thread) ---
    Shape getShape (int fn) const;
    void setShape (int fn, const Shape& shape);
    static Shape defaultShape (int fn);
    // Bumped whenever shapes change from outside the editor (state load, preset recall).
    std::atomic<int> shapesVersion { 0 };

    // --- Presets (message thread) ---
    bool isPresetUsed (int slot) const;
    void storePreset (int slot);
    void recallPreset (int slot);
    void clearPreset (int slot);
    void clearAllPresets();
    int getCurrentPreset() const { return currentPreset; }
    std::atomic<int> presetsVersion { 0 };

    // --- Meters (audio thread writes, UI reads + resets) ---
    std::array<std::atomic<float>, maxTaps> meterPre {};
    std::array<std::atomic<float>, maxTaps> meterPost {};

    static const juce::StringArray& noteNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const std::vector<double>& noteWholeFractions();

    void rebuildTable (int fn);
    float lookup (int fn, float x) const;
    double currentTimeBaseMs();

    // Shapes are edited on the message thread; the audio thread only reads the tables.
    mutable juce::CriticalSection shapeLock;
    std::array<Shape, NumFns> shapes;
    static constexpr int tableSize = 512;
    std::array<std::array<std::atomic<float>, tableSize + 1>, NumFns> tables {};

    juce::ValueTree presets { "Presets" };
    int currentPreset = 0;

    // --- DSP state ---
    double sr = 44100.0;
    juce::AudioBuffer<float> delayBuffer;
    int bufferLength = 0;
    int writePos = 0;
    int lastActiveTaps = 0;
    std::array<float, maxTaps> curDelay {}, curGainL {}, curGainR {};
    std::array<float, maxTaps> targetDelay {}, targetGainL {}, targetGainR {};
    float delaySmoothCoeff = 0.0f, gainSmoothCoeff = 0.0f;
    juce::SmoothedValue<float> drySmoothed, wetSmoothed, feedbackSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapPanZeitProcessor)
};
