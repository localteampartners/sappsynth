#pragma once
#include <cstdint>
#include "QualityMode.h"
#include "../dsp/oscillators/BandLimitedOscillator.h"
#include "../dsp/modulation/Lfo.h"

namespace sappsynth {

// Plain-value musical state. The plugin adapter fills this from host
// parameters each block; the engine owns smoothing. Field names mirror the
// stable parameter IDs in parameters/ParameterIds.h.
struct OscillatorParams
{
    Waveform waveform { Waveform::Saw };
    int octave { 0 };          // -2..+2
    int semitones { 0 };       // -12..+12
    float fineCents { 0.0f };  // -50..+50
    float pulseWidth { 0.5f };
    float level { 1.0f };      // 0..1
};

struct PatchState
{
    OscillatorParams osc1 {};
    OscillatorParams osc2 { Waveform::Saw, 0, 0, 0.0f, 0.5f, 0.0f };

    int subOctave { 1 };            // octaves below osc1 (1 or 2)
    Waveform subWaveform { Waveform::Pulse };
    float subLevel { 0.0f };
    float noiseLevel { 0.0f };

    // Audio-rate cross-modulation: osc2 output modulates osc1 frequency
    // (classic FM/clang territory; §8.5). 0..1 panel value.
    float osc2ToOsc1Fm { 0.0f };

    float mixerDrive { 1.0f };      // 1..8 linear
    float mixerCharacter { 0.1f };  // 0..1 -> asymmetry

    float cutoffHz { 12000.0f };
    float resonance { 0.1f };       // 0..1
    float filterDriveDb { 0.0f };   // -12..+24
    float keyTrack { 0.5f };        // 0..1 (1 = full tracking)
    float filterEnvAmount { 0.0f }; // -1..1 -> +-5 octaves
    float velocityToCutoff { 0.3f };

    float ampAttack { 0.005f }, ampDecay { 0.2f }, ampSustain { 0.8f }, ampRelease { 0.25f };
    float filterAttack { 0.005f }, filterDecay { 0.25f }, filterSustain { 0.3f }, filterRelease { 0.25f };

    float lfoRateHz { 1.5f };
    LfoShape lfoShape { LfoShape::Sine };
    float lfoToPitchCents { 0.0f };   // 0..100
    float lfoToCutoff { 0.0f };       // 0..1 -> +-3 octaves

    int polyphony { 8 };              // 1..16

    // Arpeggiator: 0=Off, 1=Up, 2=Down, 3=Up-Down, 4=Random.
    int arpMode { 0 };
    float arpRateHz { 8.0f };         // steps per second
    int arpOctaves { 1 };             // 1..3
    float arpGate { 0.5f };           // fraction of a step the note sounds

    // Unison stacks voice cards per note (center-preserving detune, stereo
    // spread, 1/sqrt(N) gain compensation). Eats polyphony, like hardware.
    int unisonCount { 1 };            // 1..5
    float unisonDetuneCents { 12.0f };// 0..50
    float unisonSpread { 0.7f };      // 0..1
    float glideSeconds { 0.0f };      // 0..1 (0 = off)

    // Global effects (architecture §22) — support the synth, added post-sum.
    float chorusMix { 0.0f };
    float chorusRateHz { 0.5f };
    float delayTimeS { 0.35f };
    float delayFeedback { 0.35f };
    float delayMix { 0.0f };
    float reverbSize { 0.5f };
    float reverbMix { 0.0f };

    // Analog character (architecture §9). characterAmount scales every static
    // tolerance; drift/warmup add the time-varying layers.
    float characterAmount { 0.5f };   // 0..1
    float driftAmountCents { 1.5f };  // 0..10 target stationary std-dev
    float driftSpeed { 0.5f };        // 0..1 -> mean-reversion rate
    float warmupAmount { 0.0f };      // 0..1

    float outputDriveDb { 0.0f };     // 0..24 into the output saturator
    float masterDb { -6.0f };

    QualityMode quality { QualityMode::Normal };
};

} // namespace sappsynth
