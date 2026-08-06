# SappSynth Analog DNA Expansion

You are extending the existing SappSynth project. The synthesizer architecture and initial implementation already exist. Do not replace the project, generate a separate prototype, or redesign working systems without a concrete technical reason.

Your job is to inspect the current repository, understand the existing DSP engine, voice architecture, parameter system, preset format, plugin wrappers, UI, tests, and build configuration, then add a professional virtual-analog behavior system called **SappSynth Analog DNA**.

The goal is not to add a generic “analog” knob or random pitch wobble. The goal is to make every SappSynth instance behave like one persistent physical instrument whose oscillators, filters, envelopes, VCAs, nonlinear stages, and shared electrical environment interact in convincing and musically useful ways.

SappSynth should also serve as an educational and experimental instrument. Users must be able to hear, isolate, visualize, and understand the behaviors that distinguish ideal digital synthesis from modeled analog synthesis.

## Primary product concept

Every SappSynth instance represents one individual virtual synthesizer.

Each instance has a reproducible **unit identity**, generated from a deterministic seed. Each polyphonic voice belongs to that unit and has its own persistent component characteristics.

Voice variation must not be implemented as uncorrelated random noise applied to parameters. It must be divided into distinct categories:

1. Permanent unit-level characteristics
2. Permanent voice-level component tolerances
3. Slowly changing thermal and electrical behavior
4. Note-dependent variation
5. Performance-dependent circuit behavior
6. Shared behavior affecting all voices
7. Nonlinear interactions within the audio signal path

The result should feel like a coherent physical instrument, not multiple copies of the same DSP voice with random modulation added.

## First step, inspect the existing project

Before modifying code:

1. Read the existing architecture documentation.
2. Inspect the full source tree.
3. Identify the current:

   * DSP core
   * Synth voice and voice allocator
   * Oscillator implementation
   * Mixer and gain staging
   * Filter implementation
   * Envelope implementation
   * VCA or output stage
   * Oversampling system
   * Modulation system
   * Parameter registry
   * Preset and state serialization
   * Plugin wrappers
   * UI architecture
   * Visualization or metering infrastructure
   * Test framework
4. Determine which components can be extended cleanly.
5. Identify any missing abstractions required for Analog DNA.
6. Write a concise implementation plan before making large changes.

Do not create duplicate DSP paths when the existing systems can be extended.

Do not place UI, visualization, file handling, allocation, locks, logging, or serialization work on the realtime audio thread.

Preserve compatibility with the existing plugin formats and standalone application.

## Architectural requirement

Keep Analog DNA within the framework-independent DSP engine wherever possible.

A recommended conceptual structure is:

```text
SappSynth Engine
├── UnitProfile
├── VoiceProfile[]
├── SharedCircuitState
├── DriftEngine
├── WarmupModel
├── VoiceVariationEngine
├── NonlinearMixer
├── LadderFilter
├── VCAAndOutputModel
├── AnalogDNAController
└── DiagnosticSnapshotPublisher
```

Adapt this structure to the existing repository rather than forcing these exact class names.

## Unit identity

Add a persistent unit-level profile representing the individual virtual synthesizer.

A conceptual model is:

```cpp
struct UnitProfile
{
    uint64_t seed;

    float masterTuneOffset;
    float referenceVoltageError;
    float powerSupplyStiffness;
    float globalTemperatureResponse;
    float oscillatorCoupling;
    float noiseFloor;
    float crosstalkAmount;
    float outputStageBias;

    std::array<VoiceProfile, MaxVoices> voices;
};
```

The exact fields may differ based on the existing architecture.

The unit profile must:

* Be generated deterministically from a seed
* Be serialized with plugin state and presets
* Reproduce the same sound when reloaded
* Remain stable while notes are played
* Support regeneration
* Support locking
* Support copying between presets
* Support future schema migrations

A random seed must never be regenerated merely because the editor reopened or the sample rate changed.

## Persistent voice profiles

Each polyphonic voice must have a persistent component profile.

A conceptual profile is:

```cpp
struct VoiceProfile
{
    float oscillator1Calibration;
    float oscillator2Calibration;
    float oscillator1ScaleError;
    float oscillator2ScaleError;

    float oscillator1DriftCharacter;
    float oscillator2DriftCharacter;

    float sawShapeCurvature;
    float pulseWidthBias;
    float triangleAsymmetry;

    float filterCutoffOffset;
    float filterTrackingError;
    float resonanceVariation;
    float filterStageBias[4];

    float envelopeAttackScale;
    float envelopeDecayScale;
    float envelopeSustainOffset;
    float envelopeReleaseScale;

    float vcaBias;
    float vcaGainVariation;
    float vcaBleed;
    float localNoiseLevel;
};
```

Voice 3 might consistently tune slightly sharp. Voice 7 might have a slightly darker filter. Voice 11 might have a slower attack.

These characteristics must remain associated with the physical voice slot, not be randomly regenerated for every note.

The voice allocator must expose or report which physical voice slot is currently playing each note so the UI can visualize it.

## Four timescales of instability

Implement instability across multiple timescales.

### Permanent variation

Generated from the unit seed and retained indefinitely:

* Oscillator calibration
* Oscillator scaling error
* Waveform asymmetry
* Pulse-width bias
* Filter cutoff offset
* Filter tracking error
* Filter stage bias
* Resonance variation
* Envelope timing variation
* VCA gain variation
* VCA bleed
* Noise-floor variation

### Warm-up behavior

The instrument should begin less stable and gradually settle.

Warm-up should affect related systems differently:

* Oscillator tuning settles at one rate
* Filter cutoff settles at another rate
* Shared reference voltage settles at another rate
* Output-stage bias may settle more slowly
* Noise may change slightly during warm-up

Do not implement warm-up as one global multiplier applied to random noise.

Provide a developer option to accelerate, freeze, reset, or bypass warm-up for testing.

### Continuous movement

Use smooth stochastic processes rather than ordinary periodic LFOs.

Potential methods include:

* Bounded random walks
* Low-passed noise
* Ornstein-Uhlenbeck processes
* Multi-rate stochastic modulation
* Slowly moving correlated noise
* Independent and shared drift components

Continuous drift must be:

* Sample-rate independent
* Smooth
* Bounded
* Deterministic when using the same seed
* Free of clicks and discontinuities
* Subtle at default settings
* Exaggeratable in educational mode

Do not call a general-purpose random-number generator once per sample unless profiling proves it safe and necessary. Generate efficient control-rate processes and interpolate smoothly where appropriate.

### Performance-dependent behavior

The analog model must respond to what the musician plays.

Model at least:

* Total active voice load
* Input level entering the filter
* Resonance level
* Filter drive
* Envelope retrigger state
* Rapid repeated notes
* Voice stealing
* Residual oscillator, envelope, filter, or VCA states
* Output-stage load

Repeated notes should not always begin from mathematically identical conditions unless the user selects an idealized mode.

## Shared virtual electrical environment

Add a shared circuit state used by all voices.

A conceptual model is:

```cpp
struct SharedCircuitState
{
    float supplyVoltage;
    float referenceVoltage;
    float temperature;
    float totalVoiceLoad;
    float outputLoad;
    float recoveryState;
};
```

This state should update from aggregate instrument activity.

When additional voices become active:

1. Simulated electrical load increases.
2. The virtual supply changes by a small amount.
3. Oscillator tuning, filter cutoff, saturation headroom, and output behavior respond in correlated but non-identical ways.
4. The supply gradually recovers as the load decreases.

This behavior must remain subtle in normal mode. It must not sound like obvious pitch modulation, pumping, or a global chorus.

Implement bounded responses and safe parameter ranges.

All voices must feel connected to the same machine.

## Structured correlation

Do not make every variation source independent.

Examples of useful correlation:

* Reference-voltage movement may influence all oscillator pitches
* Power-supply movement may influence filter cutoff and nonlinear headroom
* Temperature may affect oscillator tuning and filter behavior at different scaling factors
* Component tolerances remain independent per voice
* Oscillators within one voice may have partially correlated drift
* Output load may influence the final amplifier but not directly alter envelope timing

Build a small, intentional dependency graph rather than applying random values everywhere.

Document the correlation model.

## Oscillator behavior

Preserve or improve the existing band-limited oscillator implementation.

Analog DNA may influence:

* Static tuning offset
* Scale tracking error
* Slow drift
* Very slow shared drift
* Waveform curvature
* Pulse-width bias
* Triangle asymmetry
* Reset phase behavior
* Hard-sync response
* Oscillator coupling
* Signal-level variation

Do not degrade aliasing performance.

Hard sync, pulse-width modulation, waveform morphing, and oscillator modulation must remain stable at high notes and high sample rates.

Provide an ideal oscillator mode that bypasses analog variation for direct comparison.

## Nonlinear oscillator mixer

The oscillator mixer must behave as part of the sound-generation circuit, not merely as a set of digital gain controls.

Model:

* Input-dependent saturation
* Gradual soft clipping
* Headroom interaction
* Oscillator-level interaction
* Optional crosstalk
* Drive into the filter
* Gain compensation where needed

Increasing oscillator levels should change harmonic content and alter the way the filter responds.

Do not place one generic saturator after an otherwise linear signal chain and call it analog modeling.

## Ladder filter behavior

Extend or replace the existing ladder filter only if technically necessary.

The target is a stable, musically useful nonlinear ladder-style filter with:

* Four interacting stages
* Nonlinear behavior within the stages
* Resonance feedback
* Input-level-dependent saturation
* Stable self-oscillation where appropriate
* Sample-rate independence
* Smooth parameter changes
* Appropriate oversampling
* Safe operation across supported sample rates
* Protection against NaNs, infinities, denormals, and unstable feedback states

Filter variation may include:

* Per-voice cutoff offset
* Keyboard-tracking error
* Resonance variation
* Stage-specific bias
* Saturation asymmetry
* Feedback loss
* Temperature influence
* Supply influence

Do not oversample the entire synthesizer blindly. Oversample nonlinear and feedback-sensitive regions where the audible benefit justifies the CPU cost.

Provide quality settings such as:

```text
Draft
Normal
High
Ultra
```

Quality changes must not break preset compatibility.

## Envelope and retrigger behavior

Model small per-voice envelope differences:

* Attack timing
* Decay timing
* Sustain offset
* Release timing
* Curve shape
* Retrigger behavior
* Residual state

Support selectable retrigger styles where compatible with the current architecture:

* Reset from zero
* Restart from current value
* Hardware-style residual state

Avoid clicks when changing notes or stealing voices.

## VCA and output stage

Add modeled VCA and output behavior rather than ending the chain with only a linear multiply.

Possible characteristics:

* Gain variation
* Bias
* Low-level bleed
* Soft saturation
* Noise floor
* Slight asymmetry
* Output-stage headroom
* Load-dependent response

The default model must remain clean enough for professional production. Imperfections should enhance character, not make the plugin sound broken.

## Analog DNA parameter design

Add a simple primary control set.

Recommended user-facing controls:

### Analog DNA

Master amount for the entire modeled system.

At zero, the synth should approach ideal digital behavior while preserving the basic patch.

### Condition

Controls the overall range of component tolerances.

Suggested conceptual range:

```text
Factory
Used
Vintage
Worn
```

This may be a continuous parameter with named display ranges.

### Calibration

Controls how tightly oscillators, filters, envelopes, and VCAs are aligned.

### Warmth

Controls circuit operating level and nonlinear interaction. Do not implement it as a simple low-pass or static EQ tilt.

### Supply

Controls shared power-supply stiffness and load response.

### Age

Influences noise, drift range, calibration instability, crosstalk, and component behavior.

### Service

A command that recalibrates selected aspects of the instrument while preserving the underlying unit identity.

### DNA Seed

Allows the user to regenerate, enter, copy, paste, lock, or randomize the unit identity.

The normal interface should remain approachable. Detailed component controls belong in an advanced Analog DNA editor or laboratory page.

## Operating modes

Implement three comparison modes:

```text
Ideal Digital
Analog DNA
Exaggerated Demonstration
```

### Ideal Digital

Bypasses component tolerance, drift, warm-up, shared supply, and modeled instability while retaining the base oscillator, filter, envelopes, modulation, and patch.

### Analog DNA

Uses production-quality subtle modeling.

### Exaggerated Demonstration

Amplifies the selected behavior so users can clearly hear and see it.

This mode is educational. It does not need to sound like realistic hardware at extreme settings.

Switching modes must be click-free.

## X-Ray Mode

Add an educational and diagnostic interface called **X-Ray Mode**.

X-Ray Mode must allow users to isolate individual model layers:

* Permanent voice variation
* Oscillator drift
* Shared drift
* Warm-up
* Mixer saturation
* Ladder-stage nonlinearity
* Resonance feedback
* Power-supply interaction
* Envelope variation
* VCA behavior
* Noise and crosstalk
* Complete model

Users should be able to compare:

```text
Before modeling
After modeling
Difference signal
```

Any A/B or null-comparison feature must use gain matching where appropriate.

## Voice fingerprint display

Create a visualization showing each persistent physical voice.

For each voice, expose useful diagnostic values such as:

```text
Voice 1
Pitch calibration: +1.8 cents
Filter offset: +0.6 percent
Attack scale: 0.97
VCA gain: +0.2 dB
Current note: C3
Current state: active
```

The display should highlight the voice used for each played note.

Do not send large structures or allocate memory from the audio thread. Publish compact diagnostic snapshots through an existing lock-free system or add a safe single-producer, single-consumer snapshot mechanism.

## Drift timeline

Add a timeline display showing recent values for:

* Per-oscillator tuning
* Shared reference voltage
* Filter cutoff movement
* Simulated temperature
* Power-supply state
* Total voice load
* Output load

Maintain only a sensible rolling history.

The UI must not directly read mutable DSP objects owned by the audio thread.

## Signal-path inspection

Allow users to inspect key stages:

```text
Oscillator output
Mixer output
Filter input
Individual ladder stages, where practical
Filter output
VCA output
Final output
```

Provide suitable visualizations:

* Oscilloscope
* Spectrum
* Harmonic levels
* Transfer curve
* Saturation amount
* Feedback amount
* Oversampling status
* Aliasing comparison where practical

Diagnostic processing must be optional and must not impose substantial CPU cost while X-Ray Mode is closed.

## Guided experiments

Add a small experiment system or prepare the architecture for it.

Initial experiments should include:

1. Ideal oscillator versus drifting oscillator
2. Independent drift versus shared drift
3. Linear mixer versus nonlinear mixer
4. Low oscillator level versus filter overdrive
5. Linear filter versus nonlinear ladder stages
6. Component tolerance across polyphonic voices
7. Stiff supply versus responsive supply
8. Envelope reset versus residual retrigger
9. One voice repeated versus round-robin voice allocation
10. Normal modeling versus exaggerated modeling

Each experiment should:

* Load or create a controlled state
* Explain what parameter is changing
* Let the user hear an A/B comparison
* Display the relevant internal behavior
* Restore the previous state when closed

## Preset and state compatibility

Update the preset schema cleanly.

Store at least:

```json
{
  "analogDna": {
    "enabled": true,
    "seed": 38472019,
    "locked": true,
    "condition": 0.42,
    "calibration": 0.81,
    "warmth": 0.55,
    "supply": 0.38,
    "age": 0.27,
    "schemaVersion": 1
  }
}
```

Adapt this to the existing state format.

Requirements:

* Existing presets must continue loading
* Missing Analog DNA values must receive sensible defaults
* New presets must reproduce their unit identity
* State loading must never produce abrupt realtime-thread allocations
* Future migration must be possible
* Host automation must not be created for command-style actions such as randomize unless the existing parameter framework supports this safely

## Realtime safety

The audio thread must not:

* Allocate memory
* Lock mutexes
* Read files
* Write files
* Perform logging that can block
* Parse JSON
* Update UI objects
* Resize containers
* Use unpredictable system services
* Generate large diagnostic buffers

Preallocate required memory during prepare or initialization.

Use atomic values, lock-free queues, immutable snapshots, or carefully designed double buffering for communication.

All drift generators and nonlinear stages must be protected against invalid floating-point states.

## Determinism

Provide deterministic operation for tests and preset recall.

The same combination of:

* Preset state
* Analog DNA seed
* Sample rate
* Input events
* Quality mode

should produce reproducible output within reasonable floating-point tolerances.

Warm-up state may be stored or intentionally restarted, but the behavior must be clearly defined and tested.

Provide a developer mode to freeze all time-varying analog behavior while retaining static component tolerance.

## CPU strategy

Target professional realtime performance.

Use:

* Control-rate updates for very slow behavior
* Smooth interpolation between control values
* SIMD only where it clearly improves maintainability and performance
* Selective oversampling
* Cached coefficients where safe
* Preallocated buffers
* Denormal protection
* Profiling before premature optimization

Add benchmarks for:

* One voice
* Eight voices
* Sixteen voices
* Maximum supported voices
* Normal quality
* High quality
* Ultra quality
* X-Ray Mode open and closed

Report CPU usage and identify the highest-cost DSP sections.

## Testing

Add automated tests for:

### Unit profile tests

* Same seed produces the same profile
* Different seeds produce different profiles
* Values remain within safe bounds
* Profiles survive serialization
* Old preset formats migrate correctly

### Drift tests

* Drift remains bounded
* Drift is continuous
* Drift is sample-rate independent
* Shared and local drift have the intended correlation
* Frozen mode remains stable
* Reset is deterministic

### Voice tests

* Voice identity remains attached to the physical slot
* Round-robin allocation exposes expected variation
* Voice stealing is click-free
* Retrigger behavior matches the selected mode

### Filter tests

* No NaNs or infinities
* Stable at supported sample rates
* Stable at extreme cutoff and resonance values
* Stable during rapid automation
* Oversampling modes remain functional
* Self-oscillation, if supported, remains bounded

### Realtime tests

* No allocations during audio processing
* No locks during audio processing
* No UI access from the audio thread
* Diagnostic mode does not violate realtime constraints

### Audio regression tests

Render reference examples for:

* Ideal mode
* Static voice variation
* Drift
* Warm-up
* Mixer saturation
* Filter drive
* Resonance
* Shared supply response
* Envelope variation
* Complete Analog DNA mode

Store test metadata and tolerances so legitimate DSP improvements can update references intentionally.

## Implementation phases

Implement the work incrementally.

### Phase 1, repository assessment

* Document the existing architecture
* Identify integration points
* Identify risks
* Produce the implementation checklist
* Build and run the existing tests before modifying behavior

### Phase 2, deterministic profile system

* Add UnitProfile
* Add VoiceProfile
* Add deterministic profile generation
* Add seed handling
* Add serialization and migrations
* Add unit tests

### Phase 3, static voice variation

* Apply oscillator calibration
* Apply filter offsets
* Apply envelope scaling
* Apply VCA variation
* Confirm persistent voice identity
* Add voice fingerprint diagnostics

### Phase 4, structured drift and warm-up

* Add local oscillator drift
* Add shared reference drift
* Add temperature model
* Add warm-up behavior
* Add frozen test mode
* Add timeline diagnostics

### Phase 5, nonlinear interactions

* Extend nonlinear mixer
* Refine ladder-stage saturation
* Connect drive, resonance, and headroom
* Add VCA and output-stage model
* Add selective oversampling
* Add regression tests

### Phase 6, shared power supply

* Add aggregate voice load
* Add supply sag and recovery
* Correlate supply with appropriate circuit parameters
* Add safety bounds
* Add Supply control
* Add visualization

### Phase 7, X-Ray Mode

* Add isolation toggles
* Add Ideal, Analog DNA, and Exaggerated modes
* Add A/B comparison
* Add signal-path scopes
* Add spectrum and harmonic displays
* Add guided experiments

### Phase 8, optimization and polish

* Profile CPU usage
* Remove realtime hazards
* Tune default ranges
* Create professional factory presets
* Add documentation
* Run plugin validation
* Test across supported hosts and sample rates

Complete each phase in working, testable increments. Do not begin a massive rewrite spanning all phases at once.

## Required deliverables

Produce:

1. Updated source code
2. Updated architecture documentation
3. Analog DNA technical design document
4. Parameter reference
5. Preset schema documentation
6. Tests
7. Benchmarks
8. Known limitations
9. A change log
10. A manual testing checklist
11. At least ten demonstration presets
12. At least five guided X-Ray experiments

## Demonstration presets

Create presets designed to reveal the system rather than hide it under effects.

Include:

1. Ideal Mono Bass
2. Analog DNA Mono Bass
3. Warm Oscillator Lead
4. Aging Poly Chords
5. Voice Round-Robin Keys
6. Driven Ladder Bass
7. Soft Supply Pad
8. Worn Calibration Brass
9. Clean Versus Saturated Sequence
10. Maximum DNA Demonstration

Effects should be disabled or minimal in the diagnostic presets.

## Acceptance criteria

The expansion is successful when:

* Existing SappSynth functionality still works
* Existing presets still load
* A preset reloads with the same virtual unit identity
* Repeated notes can reveal persistent voice differences
* Analog behavior sounds structured rather than randomly modulated
* Large chords subtly affect the shared instrument state
* Mixer level audibly changes filter character
* Filter saturation and resonance interact naturally
* Analog DNA can be bypassed for a valid ideal comparison
* X-Ray Mode clearly shows what the model is doing
* The audio thread remains realtime safe
* The plugin remains stable at supported sample rates and buffer sizes
* Default settings sound polished and usable
* Extreme educational settings remain bounded and safe
* CPU cost scales predictably with voice count and quality mode
* Automated tests cover deterministic profiles, drift, nonlinear stability, serialization, and voice identity

## Sound-quality principles

Follow these principles throughout implementation:

* Subtle behavior should accumulate into character
* No single default imperfection should call attention to itself
* Analog modeling should affect interaction, not merely add noise
* Parameters should remain musically predictable
* Random variation should have physical and architectural meaning
* Nonlinear stages should be gain staged intentionally
* More analog character must not automatically mean darker sound
* Presets should remain mix-ready
* The instrument must still sound excellent with Analog DNA disabled
* Educational modes may exaggerate behavior, production mode should remain restrained

## Restrictions

Do not:

* Rewrite the entire project without justification
* Add a generic random wobble and label it analog
* Add one saturator at the output and label it circuit modeling
* Place diagnostic UI work on the audio thread
* Break existing preset compatibility
* Introduce unnecessary third-party dependencies
* Copy protected code from commercial synthesizers
* Claim exact hardware emulation without measurement and validation
* Use trademarked hardware branding as the main product identity
* Expand scope into effects, sequencers, sample libraries, or unrelated features before Analog DNA is complete

## Working process

After inspecting the repository, begin by reporting:

1. What already exists
2. Which systems can be reused
3. Which systems need modification
4. Any architectural conflicts
5. The exact files expected to change during Phase 2
6. The tests that will be added first

Then implement Phase 2.

After each phase:

* Build all targets
* Run relevant tests
* Report changed files
* Report completed behavior
* Report unresolved issues
* Report CPU or stability observations
* Commit the work in a logically named commit if source control is available

Do not claim completion merely because code was generated. Verify that the code builds, tests pass, plugin state reloads correctly, and the implemented behavior can be demonstrated.
