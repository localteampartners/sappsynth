# CHANGELOG — sappsynth

<!-- UPDATE WHEN: you ship a meaningful change — feature, fix, migration, dependency bump that users/operators would care about. Trivial refactors don't belong here. -->

Newest first. Format: `## YYYY-MM-DD — short title`, then bullets.

---

## 2026-08-10 — 0.10.0

- **The preset bank is calibrated at full polyphony now** (issue #1). Voices sum
  onto a bus with no headroom scaling, so a patch keeps getting louder the more
  notes you hold — but the bank had been levelled against a *four-note* chord.
  Hold eight and 10 of the 186 presets went over the -3 dBFS ship ceiling, one
  of them to full scale. `preset-audit`'s chord pass is eight notes at full
  velocity, and the bank is re-levelled against it: every preset now peaks at
  -6 dBFS on the worst case a player can actually hit.
- **The audit no longer depends on the order of the bank.** The 16 voice cards
  carry their own gain and pan tolerances, so the same chord measures up to
  3.5 dB apart depending on which cards the round-robin allocator lands on —
  and the audit inherited whatever cursor the previous preset left behind.
  `preset-audit` now pins the allocator (`SynthEngine::resetVoiceAllocation`)
  and measures the chord from four card positions, keeping the loudest.
- **What this means for your projects:** factory preset levels moved, by
  -6.1 dB at most and +10.0 dB at most, median -0.1 dB — 100 presets got
  quieter, 71 louder. The DSP is untouched, so a patch you built yourself
  sounds and measures exactly as it did. The four presets named in issue #1
  now peak at -6.2 (Dark Cathedral), -5.7 (Metal Drone), -5.6 (Glacier Pad)
  and -6.0 dBFS (Warm Tape Pad) on an eight-note chord, against -2.1, -2.6,
  -3.3 and -5.6 before.
- **`Mix Drive` reaches unity.** It ran 1..8 with a default of 1.2, so the
  mixer saturator could not be switched off and every patch shipped with it
  already engaged. The range is 0.25..8 and the default is 1.0 — transparent.
- **`Master` reaches -60 dB** (was -40). The densest pads were pinned at the
  old floor with nothing left to give. Both range changes keep every saved
  value valid, but host automation lanes written against the old ranges remap.
- New `tests/unit/test_headroom.cpp`: the whole bank under the ship ceiling at
  full polyphony, the hottest pads under it from every voice-card position,
  peak growing with note count, and `Mix Drive` reaching below unity. All five
  fail on the four-note calibration.
- New `scripts/level_presets.py` — turns a `preset-audit --trims` report into
  the bank's new `output.master.db` values, so re-levelling is reproducible
  instead of manual.
- New `source/engine/PresetPatch.{h,cpp}`: the factory bank rendered without
  JUCE, so level regressions can live in the fast unit suite.
  `preset-audit --defaults` proves it still matches the live APVTS defaults.

## 2026-08-10

- **Fixed clipping across the whole preset bank.** 95 of 186 factory presets
  clipped when played — peaks up to +19 dBFS — because nothing had ever
  measured them. Every preset is now calibrated to peak at -6 dBFS, so the
  bank is consistent and leaves headroom for the rest of the mix. This
  affected the original presets as much as the newer ones.
- New `tools/preset-audit`: renders every preset through the real processor
  and reports peak/RMS, failing above -3 dBFS. It measures four registers
  (low/mid/high single notes and a four-note chord) and takes the loudest,
  because one test signal cannot represent basses, leads and pads alike — a
  mono bass with a 300 Hz filter is nearly silent under a C4 chord. It also
  settles the preset-change transient *before* the note rather than skipping
  the note's first 200 ms, which would miss short percussive patches.
  Documented as a pre-release step in RUNBOOK.

## 2026-08-08

- **User presets** — save the sound you have and get it back. The SAVE button
  next to PRESETS captures the current parameter state to
  `~/Documents/SappSounds/presets/sappsynth/<name>.json` in the shared SappLink
  format (`sapptune/sapplink/PRESETS.md`); the preset browser lists them under
  a USER category alongside the factory bank and loads them by name. Values are
  stored NORMALISED, which is the only encoding that round-trips exactly
  through the skewed parameter ranges — verified bit-for-bit across 65
  parameters by `SappUiShot --presettest`.
- **`preset` parameter (host-automatable sound selection)** — a new
  AudioParameterChoice listing the 186 factory presets followed by the user
  presets found at construction. Sound selection can now live in a host
  automation lane and be driven over SappLink, which MIDI program change alone
  could not do (sapptune issue #13). Added LAST in the layout, so no existing
  parameter index moved; selection is applied on the message thread through the
  timer that already defers program changes. auval reports 66 global parameters
  and still passes.
- The SappLink manifest (`sapptune/sapplink/manifests/sappsynth.json`) listed
  only the first 73 factory presets while the plugin had 186 — a Claude session
  could not name the other 113. Extended to the full bank (append-only; program
  numbers 0-72 are untouched) and given the `preset` parameter under a new
  `hostParameters` key.

## 2026-08-08

- **113 new factory presets** (bank goes 73 -> 186) covering the classic
  ground: LADDER (ladder-filter monos — leads, brass, growl basses, pedal),
  POLY (Jupiter/Prophet/Juno-style strings, brass, PWM, string machine),
  FM extended to 18 (DX-style tines, bells, marimba, clav, slap bass),
  TRANCE, DANCE (hypersaw anthems, rave plucks, gates, big-room stabs),
  HOUSE, TECHNO, SYNTHWAVE, CINEMA, SFX, plus more BASS/LEAD/KEYS/PAD/
  AMBIENT staples. New presets are APPENDED, so existing MIDI program
  numbers are unchanged.
- New `scripts/check_presets.py`, wired into `verify.sh`: parses the real
  parameter ranges out of PluginProcessor.cpp and validates every preset —
  out-of-range values, duplicate names, duplicated params, and presets
  whose sound sources are all silent.
- Fixed by that check: "Gamelan Steps" asked for +17 semitones on osc 2
  (parameter stops at +12, so the interval was silently clamped) — now
  expressed as +1 octave +5 semitones, which is what it meant.

## Unreleased

<!-- Working list of changes not yet deployed. Move to a dated section on deploy. -->

- 

---

## 2026-08-06 — SappLink CC-in

- Fixed CC->parameter mapping per the sapptune SappLink manifest (20 CCs,
  any channel), slewed ~15 ms, applied via the host-automation path.
- Table-driven (SappLinkCCMap.cpp) + manifest-drift test vs vendored JSON.
- 8 manifest range corrections reported to sapptune (docs/sapplink.md).
- End-to-end render proof: SappUiShot --cctest (CC 74 sweep brightens).

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
