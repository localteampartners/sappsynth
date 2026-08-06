# CURRENT STATE — sappsynth

<!-- UPDATE WHEN: a feature ships, a deploy happens, something breaks, or something gets fixed. This file answers "what's the project like *right now*?" -->

**Last verified:** 2026-08-05

---

## What's built and working

- **DSP core** (`sappsynth_core`, framework-independent C++20): PolyBLEP
  oscillators (saw/pulse/tri/sine, plus naive renderers kept for Lab
  comparisons), nonlinear pre-filter mixer + DC blocker, ZDF ladder filter
  with per-stage tanh saturation and self-oscillation, modeled VCA, ADSR
  envelopes, LFO, 16 round-robin voice cards with oldest-active stealing.
- **Structured analog variation**: unit/voice/note component profiles from a
  deterministic seed hierarchy, correlated OU drift (0.35 unit / 0.45 voice /
  0.20 local blend), thermal warm-up. Same unit seed ⇒ bit-identical renders.
- **Quality modes**: Eco (1x), Normal (2x), High (4x) oversampled nonlinear
  island (mixer sat → ladder → VCA) via runtime-generated half-band FIRs.
- **Plugin** (JUCE 8.0.15): Standalone + VST3 + AU, custom dark UI (3 rows of
  sections + MIDI keyboard), 7 factory presets, NEW UNIT seed reroll, state
  save/restore incl. unit seed. **auval passes.**
- **Offline lab**: `sapp-render` (deterministic WAV renders), `sapp-bench`
  (CPU benchmark), FFT analyzer with alias-energy regression metric,
  `scripts/analyze_wav.py`, `scripts/compare_renders.py`.
- **Tests**: 42 Catch2 cases (~1.7M assertions): determinism, alias
  regression (PolyBLEP vs naive), filter stability 44.1–192 kHz, self-osc
  pitch, envelope timing, drift boundedness, voice stealing, quality-mode
  alias comparison.

## What's deployed

- **Environment:** local builds only; plugins copied to
  `~/Library/Audio/Plug-Ins/{VST3,Components}` on build. Distribution is the
  GitHub repo (build from source) — no binaries published yet.
- **Version / commit:** main @ latest; no tagged release yet.

## What's in progress

- Nothing mid-flight; next milestones live in TODO.md (plan phases 4–6).

## What's known broken / flaky

- CPU above plan targets (Normal/16 voices ≈ 30% of a core vs <8% goal) —
  no optimization pass yet (SIMD, block processing).
- Self-oscillation tracks cutoff within ~a musical third; not chromatically
  calibrated (plan §11.6 experiment unbuilt).
- Quality-mode switches while notes sound may click (no crossfade).
- UI verified only as "launches, auval passes, quits cleanly" — no visual
  pass yet (screen capture was blocked during the build session).

## Half-finished or abandoned

- `SynthVoice::steal()` (fast-release path) exists but the allocator restarts
  envelopes from current level instead; kept for a future two-step steal.
- No effects, unison, MPE, or Lab Mode UI — deliberately deferred, not
  half-wired. See TODO.md.
