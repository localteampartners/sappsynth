# SPEC — sappsynth

<!-- UPDATE WHEN: goals change, scope changes, non-goals change, or the target user changes -->

## What this is

SappSynth is a professional virtual-analog software synthesizer (VST3 / AU /
standalone, CLAP later) that is also an interactive laboratory for learning
how synthesizers work. One engine, two faces: Instrument Mode is a focused
two-oscillator subtractive synth with a nonlinear ladder filter and structured
analog character; Lab Mode (planned) exposes the exact same DSP for stage
soloing, ideal-vs-modeled comparison, and measured experiments.

## Why it exists

Most soft synths are either polished-but-opaque instruments or toy educational
demos with fake DSP. SappSynth's bet is that a synth whose analog behavior is
*structured* (per-unit/voice/note tolerances, correlated drift — not random
noise on parameters) and *observable* (every claim measurable) can be both a
serious instrument and the best way to understand synthesis. The full design
rationale lives in `docs/architecture.md`.

## Users

- Michael (author) — plays it, uses it as the flagship sapp* audio project.
- Musicians who want a characterful VA synth they can build from source.
- Learners who want to hear and measure *why* it sounds the way it does.

## Goals (in scope)

- Convincing VA voice: band-limited oscillators, nonlinear mixer, ZDF ladder
  with self-oscillation, modeled VCA. ✅ v0.1
- Structured, reproducible analog variation (seed hierarchy, OU drift,
  warm-up); deterministic renders for a fixed unit seed. ✅ v0.1
- Realtime-safe engine: no allocation/locks/IO on the audio thread. ✅ v0.1
- Quality modes with measurable differences (alias-energy regression). ✅ v0.1
- Plugin formats: Standalone, VST3, AU (✅ v0.1); CLAP later.
- Modulation & expression: mod matrix, audio-rate destinations, MPE, unison.
- Lab Mode: stage solo/bypass, A/B, scope/spectrum/drift views, experiment
  runner with a guided experiment library.
- Distribution from GitHub: build-from-source now, signed binaries later.

## Non-goals (explicitly out of scope)

- Not a modular workstation or multi-engine ROMpler — two excellent
  oscillators and one excellent filter, deep rather than wide.
- No wavetable/FM-synthesis engines, no sample playback.
- No copying of a specific vintage synth's name or measured clone claims.
- No mobile targets for now (core is portable by design).
- Effects never hide the dry core — chorus/delay/reverb come after the voice
  path convinces on its own.

## Success criteria

- auval + pluginval pass; stable in mainstream DAWs. (auval ✅)
- Deterministic regression renders stay bit-identical per seed. (✅ tested)
- Alias-energy tests: PolyBLEP ≥15 dB better than naive; High mode measurably
  cleaner than Eco. (✅ tested)
- CPU: 16 voices Normal <8% of one Apple-Silicon core (❌ ~30% today —
  optimization pass pending, see TODO).
- Blind level-matched listening tests favor modeled mode over ideal mode.

## Constraints

- Budget: $0 — open toolchain, no paid SDKs (AAX deferred).
- Platform: macOS first (built + validated); Windows intended, untested.
- JUCE licensing must be resolved before any commercial distribution; the
  core stays JUCE-free so the wrapper is replaceable.
- Realtime audio contract (docs/architecture.md §6) is non-negotiable.
