# CHANGELOG — sappsynth

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## Unreleased

<!-- Working list of changes not yet deployed. Move to a dated section on deploy. -->

- 

---

## 2026-08-06 — v0.2.0: vintage hardware UI, effects, unison, Lab view, CI

- Vintage panel: procedurally generated photoreal assets (walnut cheeks,
  crinkle paint, 101-frame bakelite knob filmstrips, screws) via
  `scripts/generate_ui_assets.py`; Minimoog-inspired layout; popup value
  bubbles; pilot lamp; `SappUiShot` offscreen snapshot tool.
- Effects: BBD-style chorus, tape-ish ping-pong echo, Freeverb reverb.
- Unison (1–5 cards, center-preserving detune, spread, gain comp) + glide.
- Lab view: phosphor scope + spectrum (lock-free telemetry), IDEAL/MODELED
  A/B, drift freeze.
- Click-free quality switching (waits for silence); ring-buffer FIRs;
  chunk-interpolated ladder coefficient.
- CI: core tests on macOS/Windows/Linux; macOS plugin build + pluginval
  strictness 5. 47 tests green; auval passes.
- v0.2.0 GitHub release with unsigned macOS arm64 binaries.

## 2026-08-05 — v0.1: playable synth, phases 0–3 + plugin

- Research harness: CMake project, Catch2 suite (42 cases), deterministic
  offline renderer, FFT analyzer with alias-energy metric, `sapp-render` and
  `sapp-bench` CLIs, python analysis scripts.
- Framework-independent DSP core: PolyBLEP oscillators, nonlinear mixer +
  DC blocker, ZDF ladder filter (per-stage saturation, self-oscillation),
  modeled VCA, ADSR envelopes, LFO, 16 round-robin voice cards.
- Structured analog variation: unit/voice/note seed hierarchy, correlated OU
  drift, thermal warm-up; renders are bit-reproducible per unit seed.
- Quality modes Eco/Normal/High with a 1x/2x/4x oversampled nonlinear island.
- JUCE 8.0.15 plugin: Standalone + VST3 + AU, custom dark UI, MIDI keyboard,
  7 factory presets, unit-seed reroll + persistence. auval passes.
