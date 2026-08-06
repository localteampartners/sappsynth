# TODO — sappsynth

<!-- UPDATE WHEN: a task is added, completed, or re-prioritized -->

Short running task list. For "what exists *right now*," see [CURRENT_STATE.md](CURRENT_STATE.md).
For "what's broken," also see CURRENT_STATE.md's known-issues section.

---

## Next up (doing soon, in order)

1. Visual pass on the plugin UI (open standalone, fix layout/spacing, dark-mode
   contrast, knob text boxes).
2. Phase 4 — modulation & expression: compiled mod matrix, audio-rate
   destinations (osc FM, filter FM, fast PWM), MPE, macros, tempo sync, unison.
3. CPU optimization pass: profile, remove per-sample `tan()` in the ladder
   (table or per-chunk with interpolation), SIMD voices; re-run `sapp-bench`
   against plan §25 targets.
4. Quality-mode change crossfade (click-free switching per plan §12).

## Backlog (not prioritized)

- Phase 5 — Lab Mode UI: stage solo/bypass, ideal-vs-modeled A/B (level
  matched), oscilloscope, spectrum, drift plot, voice inspector, experiment
  runner + the guided experiment library (plan §19–20).
- Phase 6 — productization: preset browser with user presets, installer +
  codesigning/notarization, GitHub Releases with prebuilt binaries, CI
  (GitHub Actions macOS/Windows matrix), pluginval in CI.
- Effects: analog-style chorus, tempo delay, reverb, after the dry core is
  convincing (plan §22).
- Self-oscillation chromatic calibration experiment (plan §11.6).
- CLAP format (JUCE 8 needs the clap-juce-extensions wrapper, or JUCE 9).
- Preset schema versioning + migration layer once presets become files
  (plan §18.3).
- Windows build + validation (untested; code is portable in intent).

## Ideas / maybe

- WebView-based Lab Mode companion/tutorials (plan §23.3 keeps native UI).
- Hard sync + oscillator cross-mod (needs BLEP-corrected sync design, §8.5).
- minBLEP/BLAMP oscillator upgrade for High mode.

---

## Done (recent, rolling)

- 2026-08-05 — Phases 0–3 shipped: research harness, deterministic DSP core,
  nonlinear voice path, structured variation; 42 tests green.
- 2026-08-05 — JUCE plugin (Standalone/VST3/AU) with custom UI + 7 presets;
  auval passes.
