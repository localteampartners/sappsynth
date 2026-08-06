#pragma once
#include <array>
#include <cstdint>
#include "PatchState.h"
#include "QualityMode.h"
#include "../dsp/oscillators/VCOModel.h"
#include "../dsp/filters/LadderFilter.h"
#include "../dsp/filters/DcBlocker.h"
#include "../dsp/nonlinear/MixerModel.h"
#include "../dsp/nonlinear/VcaModel.h"
#include "../dsp/modulation/Envelope.h"
#include "../dsp/variation/ComponentProfile.h"
#include "../dsp/variation/DriftProcess.h"
#include "../dsp/variation/RandomSource.h"
#include "../dsp/oversampling/OversamplingManager.h"

namespace sappsynth {

// Values shared across voices for one control tick, computed by the engine.
// Values shared across voices for one control tick. The grouped scales are
// the Analog DNA correlation model (documented in docs/analog-dna.md):
// Condition widens all tolerances, Calibration tightens tuning/cutoff
// specifically, Age drives drift/noise/bleed, Supply sag pushes pitch down,
// cutoff down and headroom into saturation together.
struct SharedModulation
{
    float lfoValue { 0.0f };        // -1..1
    float unitDriftNorm { 0.0f };   // ~N(0,1) correlated unit-wide drift
    float warmupCents { 0.0f };
    float characterAmount { 0.5f }; // legacy master (== staticScale base)
    bool driftFrozen { false };     // Lab: hold all drift processes still

    float staticScale { 0.5f };     // env/VCA/pan/asym/PW tolerances
    float calibScale { 0.5f };      // tuning + filter-cutoff tolerances
    float driftScale { 1.0f };      // multiplier on patch drift amount
    float noiseFloorAmp { 0.0f };   // linear amplitude of unit hiss at VCA
    float bleedScale { 0.0f };      // scales per-voice VCA bleed
    float warmthDrive { 1.0f };     // internal gain-staging factor
    float supplyPitchCents { 0.0f };// shared supply sag -> pitch
    float supplyCutoffOct { 0.0f }; // shared supply sag -> cutoff
    float supplyDrive { 1.0f };     // shared supply sag -> less headroom
};

// One complete voice card (architecture §15): owns all audio-rate state, has a
// stable identity/seed so it behaves like the same physical circuit each time
// round-robin allocation returns to it.
class SynthVoice
{
public:
    static constexpr int kControlInterval = 32;

    void prepare(double sampleRate);
    void configureIdentity(int voiceIndex, std::uint64_t unitSeed);
    void applyQuality(const ProcessingQuality& quality);

    // Per-allocation start info: unison member offsets and the glide origin
    // (previous note, or -1 for none).
    struct NoteStart
    {
        float unisonDetuneCents { 0.0f };
        float unisonPan { 0.0f };
        float unisonGain { 1.0f };
        int glideFromNote { -1 };
    };

    void noteOn(int note, float velocity, std::uint64_t noteSeedValue,
                const PatchState& patch, const UnitProfile& unit,
                const NoteStart& start);
    void noteOff();
    void steal();          // fast-release for reallocation
    void hardStop();

    bool isActive() const noexcept    { return ampEnv.isActive(); }
    bool isReleasing() const noexcept { return ampEnv.isReleasing(); }
    int currentNote() const noexcept  { return note_; }
    std::uint64_t allocationAge() const noexcept { return age_; }
    void setAllocationAge(std::uint64_t a) noexcept { age_ = a; }
    const VoiceProfile& profile() const noexcept { return profile_; }

    // Renders up to kControlInterval samples additively into left/right.
    void renderChunk(float* left, float* right, int numSamples,
                     const PatchState& patch, const UnitProfile& unit,
                     const SharedModulation& mod);

private:
    double noteWithPatchOffsets(const OscillatorParams& p) const noexcept;

    // identity + variation
    int index_ { 0 };
    std::uint64_t voiceSeed_ { 0 };
    std::uint64_t age_ { 0 };
    VoiceProfile profile_ {};
    NoteVariation noteVar_ {};
    DriftProcess voiceDrift, osc1Drift, osc2Drift;
    RandomSource noiseRng;

    // dsp
    VCOModel osc1, osc2, sub;
    MixerModel mixer;
    DcBlocker dcBlocker;
    LadderFilter filter;
    Envelope ampEnv, filterEnv;
    VcaModel vca;
    OversamplingManager oversampler;

    // runtime
    double sr { 48000.0 };
    int note_ { -1 };
    float velocity_ { 1.0f };
    int oversampleFactor { 2 };
    NoteStart start_ {};
    float glideOffsetSemis { 0.0f };

    std::array<float, kControlInterval> signalBuf {};
    std::array<float, kControlInterval> ampEnvBuf {};
    std::array<float, kControlInterval> cutoffBuf {};
};

} // namespace sappsynth
