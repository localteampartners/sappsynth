#pragma once
#include <cstdint>
#include "PatchState.h"
#include "RenderContext.h"
#include "VoiceManager.h"
#include "../dsp/modulation/Lfo.h"
#include "../dsp/variation/DriftProcess.h"
#include "../dsp/variation/ThermalModel.h"
#include "../dsp/utility/SmoothedValue.h"
#include "../dsp/effects/Chorus.h"
#include "../dsp/effects/Delay.h"
#include "../dsp/effects/Reverb.h"
#include "../telemetry/TelemetryBus.h"
#include <atomic>

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

    // Lab controls (architecture §19): UI-thread setters, read on the audio
    // thread as atomics. Ideal mode strips every modeled behavior so the
    // clean digital core can be A/B'd against the full model.
    TelemetryBus& telemetry() noexcept { return telemetryBus; }
    void setLabIdealMode(bool ideal) noexcept { labIdeal.store(ideal, std::memory_order_relaxed); }
    void setDriftFrozen(bool frozen) noexcept { labDriftFrozen.store(frozen, std::memory_order_relaxed); }
    bool labIdealMode() const noexcept { return labIdeal.load(std::memory_order_relaxed); }
    bool driftFrozen() const noexcept { return labDriftFrozen.load(std::memory_order_relaxed); }

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
    Chorus chorus;
    StereoDelay delayFx;
    Reverb reverbFx;
    int lastPlayedNote { -1 };

    SmoothedValue smCutoff, smResonance, smMixerDrive, smMaster;

    TelemetryBus telemetryBus;
    std::atomic<bool> labIdeal { false };
    std::atomic<bool> labDriftFrozen { false };

    double sr { 48000.0 };
    QualityMode activeQuality { QualityMode::Normal };
    float outputDriveLinear { 1.0f };
    bool prepared { false };
};

} // namespace sappsynth
