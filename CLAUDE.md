# TapPanZeit

Two implementations of the same effect:

- `TapPanZeit.amxd` / `tap.maxpat` — the Max for Live device (unfrozen `.amxd`: 32-byte header + patcher JSON; `tap.maxpat` is the `poly~` voice).
- `vst/` — JUCE (C++17, CMake, JUCE fetched via FetchContent, tag in `TPZ_JUCE_TAG`) port producing VST3/AU/Standalone.

## DSP facts (keep the two versions in sync)

- Taps are chained: tap N delays tap N-1's output, so tap N's delay = sum of per-tap delays 1..N. The VST uses one shared ring buffer read at cumulative offsets.
- Per-tap x = (i-1)/(N-1). Delay = timeBase × timeFn(x) × 1.25 (time function 0..1 displays as 0..125%; 100% is at y=0.8). Delays are whole samples (Max `delay~`).
- Pan is linear, pan=1 is **Left**: L gain = vol·pan, R gain = vol·(1−pan).
- Feedback = tap 1's raw (pre pan/vol) output × feedback, summed into the input.
- Dry/Wet/Feedback dials are 0–100% → linear 0..1 gain.
- Sync note values are fractions of a whole note (`noteWholeFractions()` in `PluginProcessor.cpp`, same order as the M4L `TimeNote` enum).
- Max `[function]` state is `domain, rangeMin, rangeMax, then (x, y, flag, curve)` per point; a point's curve shapes the segment ending at it; negative = fast start.

## VST build

- `cmake -S vst -B vst/build -DCMAKE_BUILD_TYPE=Release && cmake --build vst/build -j`
- `TapPanZeitTests` target is an offline impulse-response test of the processor; run it after DSP changes.
