#pragma once
#include <cstdint>
#include "RandomSource.h"

namespace sappsynth {

// Structured tolerances (architecture §9.1). Unit scope is sampled once per
// virtual instrument; voice scope once per voice card; note scope at note-on.
// All values are *nominal maximum* deviations at variation amount = 1.0; the
// engine scales them by the user's "character" amount before use.

struct UnitProfile
{
    float masterTuneCents { 0.0f };      // whole-instrument tuning bias
    float filterCutoffCents { 0.0f };    // global cutoff calibration (in cents of cutoff)
    float resonanceOffset { 0.0f };      // +- resonance calibration
    float envTimeFactor { 1.0f };        // global envelope timing bias
    float outputTilt { 0.0f };           // output-stage gain bias (dB)
    float noiseFloor { 1.0f };           // this unit's hiss level factor (0.5..1.5)
    float supplyStiffness { 0.0f };      // per-unit bias on supply sag response (+-)

    static UnitProfile generate(std::uint64_t unitSeed) noexcept
    {
        RandomSource rng(seeds::combine(unitSeed, 0xA11Cull));
        UnitProfile p;
        p.masterTuneCents   = rng.normal() * 1.5f;
        p.filterCutoffCents = rng.normal() * 30.0f;
        p.resonanceOffset   = rng.normal() * 0.015f;
        p.envTimeFactor     = 1.0f + rng.normal() * 0.02f;
        p.outputTilt        = rng.normal() * 0.25f;
        p.noiseFloor        = 1.0f + rng.normal() * 0.25f;
        p.supplyStiffness   = rng.normal() * 0.15f;
        return p;
    }
};

struct VoiceProfile
{
    float osc1TuneCents { 0.0f };
    float osc2TuneCents { 0.0f };
    float osc1TrackError { 0.0f };       // cents per octave from center
    float osc2TrackError { 0.0f };
    float pulseWidthOffset { 0.0f };
    float filterCutoffCents { 0.0f };
    float resonanceOffset { 0.0f };
    float envTimeFactor { 1.0f };        // decay/release timing (RC tolerance)
    float envAttackScale { 1.0f };       // attack-specific timing tolerance
    float envSustainOffset { 0.0f };     // sustain level bias (+-)
    float vcaGainDb { 0.0f };
    float vcaBleed { 0.0f };             // 0..1: low-level bleed when env is closed
    float panBias { 0.0f };              // -1..1, tiny
    float satAsymmetry { 0.0f };         // per-card saturation symmetry

    static VoiceProfile generate(std::uint64_t voiceSeedValue) noexcept
    {
        RandomSource rng(seeds::combine(voiceSeedValue, 0xCA4Dull));
        VoiceProfile p;
        p.osc1TuneCents     = rng.normal() * 2.5f;
        p.osc2TuneCents     = rng.normal() * 2.5f;
        p.osc1TrackError    = rng.normal() * 0.6f;
        p.osc2TrackError    = rng.normal() * 0.6f;
        p.pulseWidthOffset  = rng.normal() * 0.006f;
        p.filterCutoffCents = rng.normal() * 45.0f;
        p.resonanceOffset   = rng.normal() * 0.02f;
        p.envTimeFactor     = 1.0f + rng.normal() * 0.03f;
        p.envAttackScale    = 1.0f + rng.normal() * 0.04f;
        p.envSustainOffset  = rng.normal() * 0.015f;
        p.vcaGainDb         = rng.normal() * 0.3f;
        p.vcaBleed          = std::abs(rng.normal()) * 0.5f;
        p.panBias           = rng.normal() * 0.04f;
        p.satAsymmetry      = rng.normal() * 0.08f;
        return p;
    }
};

struct NoteVariation
{
    float pitchStartCents { 0.0f };
    float attackFactor { 1.0f };
    float phase01 { 0.0f };              // key-on phase for free-running feel
    float velocityTilt { 0.0f };

    static NoteVariation generate(std::uint64_t noteSeedValue) noexcept
    {
        RandomSource rng(seeds::combine(noteSeedValue, 0x0075ull));
        NoteVariation v;
        v.pitchStartCents = rng.normal() * 1.2f;
        v.attackFactor    = 1.0f + rng.normal() * 0.04f;
        v.phase01         = rng.nextFloat01();
        v.velocityTilt    = rng.normal() * 0.03f;
        return v;
    }
};

} // namespace sappsynth
