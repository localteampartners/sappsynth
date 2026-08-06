# ARCHITECTURE — sappsynth

<!-- UPDATE WHEN: tech stack changes, a component is added/removed, data flow changes, or a major directory is renamed -->

Deep design doc: `docs/architecture.md` (34 sections). This file is the
what-exists-now summary.

## Tech stack

- **Language / runtime:** C++20, no exceptions in the audio path
- **Framework:** JUCE 8.0.15 (pinned exact tag, FetchContent) — plugin
  adapter only; the DSP core is framework-free
- **Database:** none
- **Key libraries:** Catch2 v3.7.1 (tests); numpy/matplotlib (analysis
  scripts, optional)
- **Frontend:** custom JUCE Components + LookAndFeel (no WebView)
- **Build / package manager:** CMake ≥3.24, FetchContent for deps

## Components

- **sappsynth_core** (`source/engine`, `source/dsp`, `source/lab`) — static
  lib: voices, DSP, offline renderer, analyzer. Runs in tests/CLIs with no
  JUCE.
- **SynthEngine** — event-split block rendering (sample-accurate note
  events), control-rate shared modulation (LFO, unit drift, warm-up),
  parameter smoothing, output stage.
- **VoiceManager** — 16 fixed voice cards, round-robin allocation,
  oldest-active stealing, polyphony limit.
- **SynthVoice** — per-card identity + tolerances; VCOs → nonlinear mixer →
  DC blocker → ZDF ladder → modeled VCA inside a 1x/2x/4x oversampled island.
- **Variation system** (`source/dsp/variation`) — seed hierarchy
  (unit→voice→note), component profiles, OU drift, thermal model.
- **Plugin adapter** (`source/plugin`) — APVTS ↔ PatchState, MIDI → events,
  state blobs incl. unit seed, custom editor UI.
- **Lab/offline** (`source/lab`, `tools/`) — WAV writer, FFT analyzer,
  offline renderer, `sapp-render`, `sapp-bench`.

## Data flow

```
Host/DAW (or Standalone)
  → PluginProcessor (APVTS params → PatchState; MidiBuffer → Event[])
  → SynthEngine.process(RenderBlock)          [audio thread, no alloc/locks]
      → span-split at event offsets (§6.3)
      → per 32-sample control tick: LFO, unit drift, warm-up, smoothing
      → per active voice: pitch model + drift → oscillators (base rate)
        → oversampled island: mixer saturation → DC block → ladder → VCA
        → pan → accumulate
      → output soft drive → master gain
  → host buffers
```

## Key directories

| Path | Purpose |
|---|---|
| `source/dsp/` | Header-only DSP components (inlining-critical) |
| `source/parameters/ParameterIds.h` | Stable dotted param IDs — compatibility surface, never rename |
| `tests/unit/` | Catch2 suite; alias/determinism/stability regressions |
| `tools/` | `sapp-render`, `sapp-bench` CLIs |
| `scripts/` | Python spectrum/compare tools for renders |
| `build-core/` | JUCE-free fast loop (verify.sh); `build/` = full plugin |
| `docs/architecture.md` | The full design plan this repo implements |

## External touchpoints

- None at runtime. Build-time: GitHub (JUCE, Catch2 via FetchContent).

## Known sharp edges

- Audio-thread contract (§6): anything called from `SynthEngine::process`
  must not allocate, lock, log, or touch files. Test additions too.
- Parameter IDs are forever — add new ones, never rename (§17.1).
- `SmoothedValue` rates are prepared at control rate (sr/32), not sample rate.
- Oversampler `kMaxBaseBlock` (64) must stay ≥ `SynthVoice::kControlInterval`
  (32) or island scratch buffers overflow.
- Editor `addSection` returns references into a vector — `sections.reserve`
  in the constructor guards reallocation; keep it if sections are added.
