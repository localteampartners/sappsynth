#pragma once
#include <cstdint>
#include "PatchState.h"
#include "RenderContext.h"
#include "VoiceManager.h"
#include "../dsp/modulation/Lfo.h"
#include "../dsp/variation/DriftProcess.h"
#include "../dsp/variation/ThermalModel.h"
#include "../dsp/utility/SmoothedValue.h"

namespace sappsynth {

// The framework-independent engine (architecture §4). The plugin adapter (or
// the offline renderer, or a test) hands it a PatchState + sample-accurate
// events; it renders stereo audio. No allocation, locking, or IO after
// prepare() — the audio-thread contract in §6 applies to everything here.
class SynthEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Deterministic mode: fixing the unit seed reproduces a render exactly.
    void setUnitSeed(std::uint64_t seed);
    std::uint64_t unitSeed() const noexcept { return unitSeed_; }
    const UnitProfile& unitProfile() const noexcept { return unitProfile_; }

    // Copy of the musical state; engine applies its own smoothing. Callable
    // from the audio thread (plain struct copy).
    void setPatch(const PatchState& patch);

    void process(const RenderBlock& block);

    int activeVoiceCount() const noexcept { return voiceManager.activeVoiceCount(); }
    double sampleRate() const noexcept { return sr; }

private:
    void applyEvent(const Event& event);
    void renderSpan(float* left, float* right, int numSamples);
    void updateControl(int numSamples);

    VoiceManager voiceManager;
    PatchState patch_ {};
    PatchState effectivePatch_ {};
    UnitProfile unitProfile_ {};
    std::uint64_t unitSeed_ { 0x5EED5EEDull };

    Lfo lfo;
    DriftProcess unitDrift;
    ThermalModel thermal;
    SharedModulation sharedMod {};

    SmoothedValue smCutoff, smResonance, smMixerDrive, smMaster;

    double sr { 48000.0 };
    QualityMode activeQuality { QualityMode::Normal };
    float outputDriveLinear { 1.0f };
    bool prepared { false };
};

} // namespace sappsynth
