# CURRENT STATE — sappsynth

<!-- UPDATE WHEN: a feature ships, a deploy happens, something breaks, or something gets fixed. This file answers "what's the project like *right now*?" -->

**Last verified:** 2026-08-10 (v0.11.0 gain-staging rework: preset audit
re-levelled, 68 tests green, auval PASS)

---

## What's built and working

- **DSP core** (`sappsynth_core`, framework-free C++20): PolyBLEP oscillators
  (+ naive renderers for Lab comparison), nonlinear pre-filter mixer + DC
  blocker, ZDF ladder filter with per-stage saturation and self-oscillation,
  modeled VCA, ADSR envelopes, LFO, 16 round-robin voice cards.
- **Voice features**: unison (1–5 stacked cards, center-preserving detune,
  stereo spread, gain-compensated), glide (exponential portamento),
  velocity → amp/cutoff.
- **Effects** (post-sum, §7 order): 3-tap BBD-style chorus, tape-darkened
  ping-pong echo, Freeverb-topology reverb. Every Mix is a linear dry/wet
  crossfade; the reverb's wet path is normalised against the comb bank, so
  `Verb Size` moves the tail (0.60 s → 8.05 s) and not the level (0.6 dB).
- **Gain staging is structural, not a calibration** (issue #2, v0.11.0). The
  voice bus runs 20 dB below the drive/FX/ceiling section and the makeup is
  taken after Master, so the saturators have headroom a 16-note chord cannot
  reach; the soft knee (`dsp/nonlinear/OutputStage.h`) is bit-exact unity below
  -6 dBFS. `Out Drive` at 0 dB measures 0.00 dB, and a chord's peak grows with
  every added note instead of flattening at six.
- **Structured variation**: unit/voice/note seed hierarchy, correlated OU
  drift, thermal warm-up; fixed unit seed ⇒ bit-identical renders.
- **Quality modes**: Eco/Normal/High (1x/2x/4x island); switches wait for
  silence, so they're click-free.
- **Plugin** (JUCE 8.0.15): Standalone + VST3 + AU. **Vintage hardware UI**:
  generated walnut cheeks, crinkle panel, photoreal 101-frame filmstrip knobs
  (ivory + black bakelite), engraved sections, pilot lamp, 186 factory presets,
  NEW UNIT reroll. **auval passes.**
- **Presets, both halves**: 186 factory presets selected by MIDI program change
  or the host program API, plus USER presets saved from the SAVE button to
  `~/Documents/SappSounds/presets/sappsynth/` in the shared SappLink format
  (`sapptune/sapplink/PRESETS.md`) and loaded by name. A `preset`
  AudioParameterChoice exposes both to host automation and to a Claude session
  over SappLink. `SappUiShot --presettest` proves the round trip is exact.
- **Lab view**: phosphor scope + log spectrum from a lock-free telemetry tap,
  IDEAL/MODELED A/B, FREEZE DRIFT.
- **SappLink CC-in**: 20 MIDI CCs drive parameters per the sapptune manifest
  (any channel, ~15 ms slew, host-automation path); manifest-drift test keeps
  the repos aligned; `SappUiShot --cctest` is the end-to-end proof.
- **Tooling**: `sapp-render`, `sapp-bench`, `SappUiShot` (offscreen editor →
  PNG), `scripts/generate_ui_assets.py` (all UI art is procedural),
  analysis scripts.
- **Tests**: 68 Catch2 cases green (`./verify.sh` ≈ 40 s). **CI**: GitHub
  Actions — core tests on macOS/Windows/Linux, full plugin build + pluginval
  (strictness 5) on macOS.
- **Preset levels**: all 186 presets peak at -6 dBFS (range -7.8..-5.9, none
  above the -3 dBFS ceiling) on their worst case, measured at full polyphony
  (eight notes, full velocity) across four voice-card positions. `tools/preset-audit` is the pre-release gate (~7 min);
  `tests/unit/test_headroom.cpp` is the fast guard inside verify.sh.

## What's deployed

- GitHub repo (build from source) + v0.2.0 release with unsigned macOS arm64
  binaries (Standalone/VST3/AU). Local builds copy plugins into
  `~/Library/Audio/Plug-Ins/`. Current source version is 0.11.0 (untagged; v0.10.0 is the
  latest tag).

## What's known broken / flaky

- CPU above plan §25 targets (Normal/16 voices ≈ 30% of one core vs <8%);
  ring-buffer FIR + interpolated-G passes moved little — needs a real
  profile + SIMD pass.
- Self-oscillation tracks cutoff within ~a third; §11.6 chromatic calibration
  unbuilt.
- Release binaries are unsigned/unnotarized — Gatekeeper quarantine applies
  (`xattr -dr com.apple.quarantine`). Signing needs a Developer ID.
- Windows plugin build untested (core tests run in CI; plugin job is
  macOS-only).
- **Nothing has been listened to.** Every level claim in this project is
  measured offline (peak/RMS/decay); no one has heard v0.11.0's drier reverb or
  its cleaner chords. A DAW play-test is item 2 in TODO.md.
- The plugin no longer bounds its own output: with the ceiling moved 20 dB
  above the bus nominal (DECISIONS.md 2026-08-10), a user who cranks Master on
  a levelled patch will leave full scale where the old engine soft-clipped.
  Deliberate — a fader should be a fader — but it is a behaviour change.

## Half-finished or abandoned

- `SynthVoice::steal()` fast-release path unused (allocator restarts
  envelopes from current level instead); kept for a future two-step steal.
- MPE, mod matrix beyond the fixed routes, tempo-synced LFO/delay, stage
  solo/bypass and the experiment runner — deferred, see TODO.md.

## 2026-08-10 — preset levels

Presets are levelled by `tools/preset-audit` and re-levelled with
`scripts/level_presets.py` (see RUNBOOK). Two rules the audit has to keep: the
chord pass is eight notes at full velocity, and the voice allocator is pinned
and swept — without the second, every number depends on the ORDER of the bank.

## 2026-08-08 — preset library

- 186 factory presets in 19 categories (source/plugin/FactoryPresets.cpp),
  all reachable by MIDI program change. Append new presets at the END:
  program numbers are the bank index and hosts save them in projects.
- `python3 scripts/check_presets.py` validates the bank against the live
  parameter ranges; it runs as part of `./verify.sh`.
