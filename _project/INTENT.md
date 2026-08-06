# Intent — sappsynth

<!-- Captured at scaffold time from the sappcode spec. Read this on first
     Claude run to seed SPEC.md, ARCHITECTURE.md, and TODO.md, then leave
     this file alone as the original brief. -->

SappSynth is a two-oscillator subtractive virtual-analog synthesizer built as a framework-independent C++20 DSP core wrapped in a thin JUCE 9 plugin layer, targeting VST3, Audio Unit, CLAP, and a standalone app. It models circuit-like nonlinearity (ladder filter stage saturation, nonlinear mixer, modeled VCA) and structured analog imperfection (per-unit/voice/note variation, correlated drift, thermal warm-up) on top of a band-limited, sample-accurate, allocation-free audio engine. It doubles as an interactive laboratory: every stage can be soloed, bypassed, compared ideal-vs-modeled, visualized, and measured via reproducible experiments. Distributed via GitHub; success is a focused, measured instrument that passes pluginval/auval and blind listening tests — full plan in docs/architecture.md (copied from ~/apps/SappSynth_Architecture_Plan.md).
