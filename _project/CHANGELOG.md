# CHANGELOG — sappsynth

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## Unreleased

<!-- Working list of changes not yet deployed. Move to a dated section on deploy. -->

- 

---

## 2026-08-06 — v0.5.0: preset browser + 75 presets (released with Analog DNA)

- Searchable scrollable preset browser (filter by name/category).
- Bank grown to ~75 presets; new AMBIENT + RHYTHM (generative noise)
  categories, 12 musical arp patches. Bank moved to FactoryPresets.cpp.
- Release includes macOS arm64 + Windows x64 binaries; CI + pluginval green.

## 2026-08-06 — v0.4.0: Analog DNA expansion

- Shared circuit state: aggregate voice load sags a virtual supply (80 ms
  attack / 500 ms recovery) that pushes pitch, cutoff and headroom together.
- DNA macro panel: DNA / Condition / Calibration / Warmth / Supply / Age with
  a documented correlation graph (docs/analog-dna.md).
- Extended profiles: per-voice attack scale, sustain offset, VCA bleed;
  per-unit noise floor and supply stiffness.
- Three operating modes: Ideal Digital / Analog DNA / Exaggerated
  Demonstration (Lab cycle button); seed LOCK; voice fingerprint cells +
  supply/drift timeline in the Lab.
- 10 DNA demonstration presets; 6 new tests (57 total); auval green.

## 2026-08-06 — v0.3.0: matte knobs + scales, arpeggiator, FM, 26 presets

- Knob redesign: matte solid bakelite (no gloss), full-length pointers, and
  graduated 0-10 tick/numeral scales drawn around every knob.
- Fine control: 320px drag throw, cmd/ctrl-drag ultra-fine, double-click
  resets to default.
- Arpeggiator: Up / Down / UpDown / Random, 0.5-20 Hz, 1-3 octaves, gate.
- Osc2 -> Osc1 audio-rate FM (clamped, no through-zero) for FM-style EPs,
  bells, clangs.
- Preset bank grown to 26 in categories: BASS / LEAD / KEYS / FM / PAD / ARP
  (Model Growl, Taurus Rumble, Hoover Rave, FM E-Piano, FM Bells, Jupiter
  Sweep Pad, Polysix Strings, Berlin School, Disco Octaves, ...).
- 51 tests green (arp rate/order/stop, FM sidebands); auval + pluginval pass.
- v0.2.0 release also gained Windows x64 VST3/Standalone from CI.

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
