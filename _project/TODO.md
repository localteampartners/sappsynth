# TODO — sappsynth

<!-- UPDATE WHEN: a task is added, completed, or re-prioritized -->

Short running task list. For "what exists *right now*," see [CURRENT_STATE.md](CURRENT_STATE.md).
For "what's broken," also see CURRENT_STATE.md's known-issues section.

---

## Next up (doing soon, in order)

1. CPU profiling pass (Instruments), then targeted SIMD/block optimization of
   the ladder solver + oversampling island; goal: Normal/16 voices <8%.
2. Play-testing session in a DAW (Logic/Live/Reaper) — preset polish by ear,
   gain staging across presets, keyboard velocity feel.
3. Full modulation matrix (compiled routes per §16) + tempo sync for LFO and
   echo (host BPM already reachable via the adapter).
4. MPE / note expression.

## Backlog (not prioritized)

- Lab Mode phase 2: per-stage solo/bypass debug contract, drift plot, voice
  inspector, experiment runner + guided experiment library (§19–20).
- Self-oscillation chromatic calibration experiment (§11.6).
- Windows plugin CI job + validation; installer.
- Code signing + notarization (needs Developer ID), signed releases.
- CLAP via clap-juce-extensions or the JUCE 9.x upgrade.
- Preset schema versioning + migration once user preset files land (§18.3).
- minBLEP/BLAMP oscillators for High mode; hard sync + cross-mod (§8.5).
- User preset browser (save/load/tag).

## Ideas / maybe

- WebView Lab companion/tutorials (§23.3).
- "Vintage wear" knob variation — per-unit cosmetic aging tied to unit seed.

---

## Done (recent, rolling)

- 2026-08-06 — Vintage hardware UI (generated photoreal assets), Lab view
  (scope/spectrum, IDEAL A/B, drift freeze), SappUiShot snapshot tool.
- 2026-08-06 — Effects (chorus/echo/reverb), unison, glide; click-free
  quality switching; CI (3-OS core tests + macOS pluginval); v0.2.0 release.
- 2026-08-05 — Phases 0–3: research harness, deterministic DSP core,
  nonlinear voice path, structured variation; JUCE plugin, auval green.
