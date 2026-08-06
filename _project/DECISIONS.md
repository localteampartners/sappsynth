# DECISIONS — sappsynth

<!-- UPDATE WHEN: you make a non-obvious choice (library pick, architectural pattern, tradeoff). One entry per decision, newest at top. -->

The *why* behind choices that aren't self-evident from the code. The #1 question
future-you will ask is "why did I do it this way?" — answer it here, once, when
it's fresh.

Skip obvious decisions ("I used Express because it's a Node web framework").
Write decisions where someone smart would reasonably pick differently.

---

## Format

```
## YYYY-MM-DD — short title

**Decision:** what you chose.
**Context:** the situation that forced the choice.
**Alternatives considered:** what else was on the table, and why they lost.
**Tradeoffs:** what this choice costs you.
**Revisit if:** the condition that would make you reconsider.
```

---

## Entries

## 2026-08-06 — All UI art is generated, not sourced

**Decision:** every visual asset (walnut, panel, knobs, screws) is rendered by
`scripts/generate_ui_assets.py` (numpy/PIL, committed PNGs) using the classic
filmstrip technique — 101 frames per knob, fixed lighting, rotating pointer.
**Context:** the brief asked for photoreal vintage knobs; stock images carry
license risk and can't be re-tuned.
**Alternatives considered:** procedural drawing in JUCE Graphics (looked flat,
per-frame cost at paint time); commissioned/stock art (license + iteration
friction); SVG (can't do photoreal shading well).
**Tradeoffs:** ~2.5 MB of PNGs in the repo/binary; regenerating needs the
`.venv-assets` python env.
**Revisit if:** a designer supplies real hardware photography, or binary size
starts to matter.

## 2026-08-06 — Quality switches wait for silence instead of crossfading

**Decision:** a quality-mode change is deferred until no voices are active.
**Context:** §12 demands click-free switches; crossfading two prepared render
paths doubles voice CPU during transitions and complicates state.
**Tradeoffs:** the change doesn't take effect while notes are held.
**Revisit if:** users complain; the crossfade design is the proper fix.

## 2026-08-05 — JUCE 8.0.15, not 9.0.0

**Decision:** pin JUCE to exact tag 8.0.15 via CMake FetchContent.
**Context:** docs/architecture.md recommends JUCE 9, but 9.0.0 shipped weeks
before this build and its AudioProcessor/CLAP work is still moving.
**Alternatives considered:** JUCE 9.0.0 (API churn risk, no field history);
vendoring JUCE as a git submodule (heavier repo, same pin effect).
**Tradeoffs:** no built-in CLAP; CLAP needs clap-juce-extensions or the 9.x
upgrade later. The plugin adapter is deliberately thin so the swap is cheap.
**Revisit if:** JUCE 9.1+ lands with stable notes, or CLAP becomes a launch
requirement.

## 2026-08-05 — Header-only DSP core, engine-only .cpp files

**Decision:** DSP components (`source/dsp/`) are header-only; only engine
orchestration and lab code compile as translation units.
**Context:** per-sample inner loops (ladder solver, polyBLEP, OU drift) want
inlining; the plan's repo sketch listed .cpp files per component.
**Alternatives considered:** .cpp per class (slower hot loops or LTO reliance).
**Tradeoffs:** slightly slower incremental compiles when a hot header changes.
**Revisit if:** compile times start hurting or the core grows a public ABI.

## 2026-08-05 — Fixed-point-iteration ZDF ladder (not closed-form, not Huovilainen)

**Decision:** TPT one-pole stages with tanh saturation, global feedback solved
by 1–3 fixed-point iterations (iteration count = quality mode).
**Context:** plan §11 demands ZDF behavior, per-stage nonlinearity, and
solver-as-quality-knob; needed something stable at 192 kHz on day one.
**Alternatives considered:** Huovilainen explicit model (needs its own tuning
polynomials, drifts at high cutoff); full Newton solver (2–3× cost for
inaudible gains at 2x/4x oversampling); linear ladder + output tanh (fails
the "input level changes resonance character" requirement).
**Tradeoffs:** Eco (1 iteration) approximates one-sample-delayed feedback;
self-osc pitch tracks cutoff loosely (~third) pending calibration.
**Revisit if:** the §11.6 chromatic-calibration experiment demands tighter
tracking, or Research mode wants a reference solver.

## 2026-08-05 — Triangle = leaky-integrated PolyBLEP square

**Decision:** render triangle by integrating the band-limited square through a
leaky integrator (gain 4·increment, leak 0.9995).
**Context:** proper BLAMP correction is fiddly; the integrator inherits the
square's alias suppression with three lines of code.
**Tradeoffs:** slight low-frequency droop from the leak; amplitude settles
over the first cycles after note-on.
**Revisit if:** High mode adopts real BLAMP/minBLEP oscillators.

## 2026-08-05 — sapp.yml monitor stays paused

**Decision:** no health checks; monitor entry status `paused`.
**Context:** desktop audio plugin distributed via GitHub — nothing hosted.
**Revisit if:** binaries get a release/update endpoint worth monitoring.
