# TapPanZeit (Max for Live)

- `TapPanZeit.amxd` is an unfrozen `.amxd` (32-byte header + patcher JSON); `tap.maxpat` is the `poly~` voice (one per tap).
- GitHub releases of this repo are consumed by the `plugins` website (`../plugins`), so only publish M4L device releases here.
- The VST3/AU port lives in a separate repo, `../juce-TapPanZeit` ([zsteinkamp/juce-TapPanZeit](https://github.com/zsteinkamp/juce-TapPanZeit)). Its `CLAUDE.md` documents the DSP behavior derived from this patch; keep the two in sync when changing the device's audio behavior.
