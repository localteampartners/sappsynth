# SappSynth Analog DNA — Technical Design

Implements the expansion specified in `dna.md`, layered onto the existing
structured-variation system (docs/architecture.md §9). One instance = one
persistent virtual instrument; every behavior below is deterministic per unit
seed.

## What maps where

| dna.md concept | Implementation |
|---|---|
| UnitProfile / VoiceProfile | `source/dsp/variation/ComponentProfile.h` (extended: noise floor, supply stiffness, per-segment envelope scales, sustain offset, VCA bleed) |
| DriftEngine | `DriftProcess` (OU), blended 0.35 unit / 0.45 voice / 0.20 per-osc |
| WarmupModel | `ThermalModel` |
| SharedCircuitState | supply-sag model in `SynthEngine::updateControl` |
| AnalogDNAController | DNA macro block in `SynthEngine::updateControl` -> `SharedModulation` grouped scales |
| DiagnosticSnapshotPublisher | `SynthEngine::Diagnostics` (relaxed atomics) + `TelemetryBus` |
| X-Ray | Lab panel: scope, spectrum, voice fingerprint cells, supply/drift timeline, mode cycle, drift freeze |

## Correlation model (the dependency graph)

```
DNA Amount (variation.amount) ── master gate for everything below
│
├─ Condition (dna.condition) ──► staticScale = amount × (0.5 + 1.5·condition)
│     env timing, sustain offset, VCA gain/pan/asymmetry, PW bias, note variation
│
├─ Calibration (dna.calibration) ──► calibScale = staticScale × (1.5 − calibration)
│     oscillator tuning + tracking, unit master tune, filter cutoff/resonance offsets
│     (tuning and cutoff misalign TOGETHER — that is what "out of calibration" means)
│
├─ Age (dna.age) ──► driftScale = 0.4 + 1.6·age   (multiplies drift cents)
│                ──► noiseFloorAmp = age × amount × unit.noiseFloor × 4e-4
│                ──► vca bleed = voice.vcaBleed × age × amount × 0.006
│
├─ Warmth (dna.warmth) ──► warmthDrive = 0.8 + 1.4·warmth
│     multiplies mixer saturation drive (internal gain staging, NOT an EQ tilt)
│
└─ Supply (dna.supply) ──► shared circuit state, updated per control tick:
      load     = activeVoices / 16
      softness = 1 − supply + unit.supplyStiffness·amount   (clamped 0..1)
      sag      → target load·softness·0.6, attack ≈ 80 ms, recovery ≈ 500 ms
      sag · amount ──► pitch −3 c/unit, cutoff −0.12 oct/unit, drive ×(1+0.35·sag)
      (correlated but differently scaled destinations; deterministic, no RNG)
```

Envelope timing tolerances are per-voice; attack has its own tolerance on top
of the shared RC factor. Output load does not alter envelope timing (per spec).

## Operating modes

`SynthEngine::LabMode`: **Ideal Digital** (all scales forced to 0, mixer/output
drive neutral), **Analog DNA** (production values above), **Exaggerated
Demonstration** (static ×4, drift ×~6 via scale caps, supply ×4, noise ×4 —
bounded by clamps, educational only). Mode changes apply at control rate and
are click-free. `FREEZE DRIFT` holds every OU process at its current value.

## Unit identity

Seed lives in plugin state (`unitSeed`) plus `unitSeedLocked`; NEW UNIT is
disabled while locked. Existing presets load unchanged — missing DNA
parameters take their defaults (APVTS handles this), and pre-DNA state blobs
simply lack the lock flag (defaults to unlocked).

## Realtime safety

The supply model, macro scales and diagnostics are computed once per 32-sample
control tick — no allocation, no RNG per sample (noise floor uses the voice's
existing deterministic generator), no locks. UI reads relaxed atomics and
immutable profile structs.

## Known limitations / deferred

- No Ultra quality tier: appending a 4th choice to `quality.mode` would shift
  the stored normalized values of existing presets (spec forbids breaking
  them). A future `quality.mode2` parameter with migration is the clean path.
- Per-ladder-stage bias arrays, retrigger-style selector, Service command,
  guided experiment runner, crosstalk: deferred; the profile fields and
  correlation graph leave room for them.
- Warm-up state restarts per prepare() (documented behavior; not serialized).

## Parameter reference (new in v0.4.0)

| ID | Name | Range | Default |
|---|---|---|---|
| `variation.amount` | DNA Amount (master) | 0..1 | 0.5 |
| `dna.condition` | Condition (Factory→Worn) | 0..1 | 0.35 |
| `dna.calibration` | Calibration (1 = tight) | 0..1 | 0.8 |
| `dna.warmth` | Warmth (operating level) | 0..1 | 0.4 |
| `dna.supply` | Supply (1 = stiff) | 0..1 | 0.7 |
| `dna.age` | Age (noise/drift/bleed) | 0..1 | 0.25 |

Ten DNA demonstration presets ship in the DNA preset category (Ideal Mono Bass
… Maximum DNA Demo), with effects minimal per spec.
