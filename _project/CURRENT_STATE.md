# CURRENT STATE — sappsynth

<!-- UPDATE WHEN: a feature ships, a deploy happens, something breaks, or something gets fixed. This file answers "what's the project like *right now*?" -->

**Last verified:** 2026-08-06

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
  ping-pong echo, Freeverb-topology reverb.
- **Structured variation**: unit/voice/note seed hierarchy, correlated OU
  drift, thermal warm-up; fixed unit seed ⇒ bit-identical renders.
- **Quality modes**: Eco/Normal/High (1x/2x/4x island); switches wait for
  silence, so they're click-free.
- **Plugin** (JUCE 8.0.15): Standalone + VST3 + AU. **Vintage hardware UI**:
  generated walnut cheeks, crinkle panel, photoreal 101-frame filmstrip knobs
  (ivory + black bakelite), engraved sections, pilot lamp, 8 factory presets,
  NEW UNIT reroll. **auval passes.**
- **Lab view**: phosphor scope + log spectrum from a lock-free telemetry tap,
  IDEAL/MODELED A/B, FREEZE DRIFT.
- **Tooling**: `sapp-render`, `sapp-bench`, `SappUiShot` (offscreen editor →
  PNG), `scripts/generate_ui_assets.py` (all UI art is procedural),
  analysis scripts.
- **Tests**: 47 Catch2 cases green. **CI**: GitHub Actions — core tests on
  macOS/Windows/Linux, full plugin build + pluginval (strictness 5) on macOS.

## What's deployed

- GitHub repo (build from source) + v0.2.0 release with unsigned macOS arm64
  binaries (Standalone/VST3/AU). Local builds copy plugins into
  `~/Library/Audio/Plug-Ins/`.

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

## Half-finished or abandoned

- `SynthVoice::steal()` fast-release path unused (allocator restarts
  envelopes from current level instead); kept for a future two-step steal.
- MPE, mod matrix beyond the fixed routes, tempo-synced LFO/delay, stage
  solo/bypass and the experiment runner — deferred, see TODO.md.
