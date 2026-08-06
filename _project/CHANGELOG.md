# CHANGELOG — sappsynth

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## Unreleased

<!-- Working list of changes not yet deployed. Move to a dated section on deploy. -->

- 

---

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
