# SappSynth

<!-- UPDATE WHEN: the one-line description changes, or the repo's top-level layout changes -->

A serious virtual-analog synthesizer that lets you hear, inspect, and
understand why it sounds the way it does. VST3 · Audio Unit · Standalone —
C++20, JUCE 8, CMake.

Two oscillators (PolyBLEP), a nonlinear zero-delay-feedback ladder filter
with self-oscillation, a modeled VCA, and **structured** analog character:
every virtual unit has its own manufacturing tolerances, every voice card its
own circuit, every note its own micro-variation, all driven by a
deterministic seed hierarchy — same seed, bit-identical render.

## Build

Requires CMake ≥3.24 and a C++20 compiler (Xcode clang tested). JUCE 8.0.15
and Catch2 are fetched automatically.

```bash
git clone https://github.com/localteampartners/sappsynth.git
cd sappsynth
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

Artefacts land in `build/SappSynthPlugin_artefacts/Release/` (Standalone
app, `SappSynth.vst3`, `SappSynth.component`) and are copied into
`~/Library/Audio/Plug-Ins/` on macOS. `auval -v aumu Spsy Ltpr` passes.

Fast development loop (core + tests, no JUCE):

```bash
./verify.sh
```

## Offline tools

```bash
./build-core/sapp-render out.wav --note 48 --res 0.9 --quality high --seed 1001
./build-core/sapp-bench                      # CPU per quality mode
python3 scripts/analyze_wav.py out.wav       # spectrum plot
python3 scripts/compare_renders.py a.wav b.wav
```

## Design

The whole build follows [docs/architecture.md](docs/architecture.md) — DSP
core with no framework dependency, realtime-safe audio thread contract,
quality modes as prepared configurations, alias-energy regression tests.
Current status: `_project/CURRENT_STATE.md`; roadmap: `_project/TODO.md`.

## Project documentation

All orientation docs live in [`_project/`](_project/). Start with
[_project/README.md](_project/README.md) — it's a 1-page index into everything
else (spec, architecture, current state, runbook, infra, decisions, etc.).

If you're an agent opening this repo, read [CLAUDE.md](CLAUDE.md) first.
