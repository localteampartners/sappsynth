# RUNBOOK — sappsynth

<!-- UPDATE WHEN: any command here stops working, or a new operational task becomes routine enough to document -->

The authoritative source for "how do I operate this thing?"

---

## Run locally

### One-time setup

```bash
git clone https://github.com/localteampartners/sappsynth.git
cd sappsynth
# nothing else — CMake fetches JUCE 8.0.15 + Catch2 on first configure
```

### Build everything (plugin + standalone + tools)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

- Standalone app: `build/SappSynthPlugin_artefacts/Release/Standalone/SappSynth.app`
- VST3 / AU are also copied to `~/Library/Audio/Plug-Ins/{VST3,Components}`.

### Start the app

```bash
open build/SappSynthPlugin_artefacts/Release/Standalone/SappSynth.app
```

### Run tests (fast loop, no JUCE)

```bash
./verify.sh
```

### Validate the plugin

```bash
auval -v aumu Spsy Ltpr        # Audio Unit validation (passes as of v0.1)
```

### Offline render / benchmark

```bash
./build-core/sapp-render out.wav --note 48 --res 0.9 --quality high --seed 1001
./build-core/sapp-bench
```

---

## Deploy

No hosted service. "Deploy" = push to GitHub; users build from source.
Binary releases (signed/notarized) are a TODO — see `_project/TODO.md`.

### Rollback

```bash
git revert <sha> && git push   # or /rollback via sapp-snapshot for suite-wide bookmarks
```

---

## Debug checklist

1. `./verify.sh` — does the core still pass its 42 test cases?
2. Rebuild clean if CMake cache is weird: `rm -rf build build-core` then reconfigure.
3. Plugin not showing in a DAW: check `~/Library/Audio/Plug-Ins/{VST3,Components}`
   timestamps, then `auval -v aumu Spsy Ltpr`; kill the AU cache with
   `killall -9 AudioComponentRegistrar` and rescan in the DAW.
4. Sounds wrong vs sounds broken: render the same note twice with a fixed seed
   (`sapp-render a.wav --seed 7 && sapp-render b.wav --seed 7`) and
   `python3 scripts/compare_renders.py a.wav b.wav` — non-identical output
   means a determinism regression, which is a bug, full stop.
5. CPU spikes: `./build-core/sapp-bench` and compare against the numbers in
   CURRENT_STATE.md.

## Preset levels (run before any release that touches the bank)

    cmake --build build --target preset-audit
    ./build/preset-audit_artefacts/Release/preset-audit

Renders every factory preset across four registers (low/mid/high single
notes plus a four-note chord) and reports peak/RMS. It exits non-zero if any
preset peaks above -3 dBFS. The bank is calibrated so every preset peaks at
-6 dBFS, which is what stops presets clipping the moment they are loaded.
Not in verify.sh: it takes ~3 minutes, well past that script's budget.

`--trims` prints `name|peak|master` for re-calibration.
