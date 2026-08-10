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

## 2026-08-10 — Bracket the nonlinear stages with headroom instead of turning the whole engine down

**Decision:** fix issue #2's polyphony flattening by scaling the summed voice
bus down 20 dB (`SynthEngine::kBusHeadroom`) into the drive/FX/ceiling section
and taking the makeup back **after** the Master fader (`kBusMakeup`). The
nonlinear stages get 20 dB of headroom; the end-to-end gain structure does not
move.
**Context:** voices sum with a single-note peak near -2 dBFS, so a chord ran
straight into the output drive's `tanh()` and stopped growing at six notes.
The obvious fix — lower the per-voice level and let the bank's Master trims
make it up — does not fit: `master` is -60..+6 dB and hosts have saved
normalised values against that range, so widening it would silently re-gain
every existing session, and leaving it alone would push ~half the bank through
the +6 clamp. Bracketing solves both: the saturator sees a quiet bus, the
fader sees the level it always saw. With it, the re-level converged in a
single audit pass and only 2 of 186 presets hit the clamp (by 1.8 and 0.7 dB).
**Alternatives considered:** widening the Master range (breaks saved
automation); lowering `SynthVoice::kVoiceMakeup` (same clamp problem, and it
would also change per-voice character, which was not at fault); a limiter after
Master (re-introduces exactly the flattening being removed, since the level is
genuinely hot before the fader).
**Tradeoffs:** the plugin no longer bounds its own output — a user who cranks
Master past a levelled patch will leave full scale, where the old engine would
have soft-clipped. That is a fader doing its job, but it is a behaviour change.
`Out Drive` also needs more of its range before it audibly saturates, because
it is now an honest gain into a knee that starts 20 dB up.
**Revisit if:** the drive control feels dead in its lower half, or hosts show
users clipping they used to be protected from.

## 2026-08-10 — Effect Mix is a LINEAR crossfade, not equal-power

**Decision:** all three effects use `out = (1-m)·dry + m·wet`.
**Context:** issue #2's requirement is that raising Mix cannot raise the level.
Equal-power (`cos/sin`) only holds that for *uncorrelated* wet paths; the
chorus and the delay's first taps are strongly correlated with the dry signal,
so equal-power would put up to +3 dB in the middle of the control — a smaller
version of the fault being fixed.
**Tradeoffs:** on the reverb, where the wet path really is uncorrelated, a
linear crossfade dips ~3 dB at Mix 0.5. Audible as a slight loss of body
mid-sweep; measurable, predictable, and cheaper than a control that lies.
**Revisit if:** reverb Mix sweeps read as a dip in practice — the fix would be
per-effect laws, not one law everywhere.

## 2026-08-10 — Normalise the reverb against comb feedback AND damping

**Decision:** divide the comb feed by `sqrt(P)` where
`P = 1 + f²(1-d)²/sqrt(A² - 4d²)`, `A = 1 + d² - f²(1-d)²` — the closed-form
broadband power gain of one damped comb.
**Context:** sappedal v0.14 fixed the same class of bug for Sapprack by
normalising its wet path against the comb bank rather than clamping the output.
The pattern transfers; the constant does not. sappsynth's combs carry a
damping one-pole in the feedback path, which flattens the gain-vs-feedback
curve a lot: measured, Size 0 → 1 moves the wet level only 3.4 dB, where the
undamped `1/sqrt(1-f²)` model predicts 11 dB. Normalising with the undamped
model would have over-corrected the cathedral end by ~7 dB and made big rooms
quieter than small ones.
**Alternatives considered:** the `(1-f²)^0.25` prototype recorded in issue #2
(empirical, still 5.5 dB of tilt against a 3.4 dB fault); a fitted polynomial
in Size (works, explains nothing, breaks if the Size→feedback map moves).
**Tradeoffs:** the four Schroeder sections in this topology are not unity-gain
(+7.3 dB over the chain) and the eight combs' `sqrt(8)` is also constant, so
both live in a single measured trim, `kWetReference = 0.0655`. If the tunings
or the section count change, that constant has to be re-measured — the test
suite is what catches it.
**Revisit if:** damping becomes a user parameter (it is fixed at 0.4), which
would make the model's `d` term matter dynamically.

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
