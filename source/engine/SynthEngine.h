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
    // Voice-bus headroom (issue #2, fault 4). Voices sum with a single-note
    // peak near -2 dBFS, so a chord ran straight into the output drive's
    // saturator: the peak of a Dark Cathedral chord stopped growing at SIX
    // notes and sat at +3.4 dBFS all the way to twelve — 15 dB of soft
    // clipping the engine applied to itself.
    //
    // The nonlinear tail of the chain (drive -> FX -> ceiling) is bracketed:
    // the summed bus is scaled DOWN by kBusHeadroom going in and the makeup is
    // taken after the Master fader. So the saturator sees 20 dB of headroom
    // and a 16-note chord never reaches it, while the end-to-end gain
    // structure — and therefore the Master parameter's -60..+6 dB range, which
    // hosts have saved in sessions — is untouched.
    static constexpr float kBusHeadroom = 0.1f;    // -20 dB into the nonlinearities
    static constexpr float kBusMakeup   = 10.0f;   // and back out after Master

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Silence everything AND park the voice allocator on a known card, so a
    // measurement does not inherit the cursor position the previous notes left
    // behind (see VoiceManager::resetAllocation).
    void resetVoiceAllocation(int startCard = 0) noexcept
    {
        voiceManager.resetAllocation(startCard);
    }

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
    // thread as atomics. Modes: 0 = Ideal Digital (strips every modeled
    // behavior), 1 = Analog DNA (production), 2 = Exaggerated Demonstration.
    enum class LabMode { Ideal = 0, Dna = 1, Exaggerated = 2 };

    TelemetryBus& telemetry() noexcept { return telemetryBus; }
    void setLabMode(LabMode mode) noexcept { labMode.store(static_cast<int>(mode), std::memory_order_relaxed); }
    LabMode currentLabMode() const noexcept { return static_cast<LabMode>(labMode.load(std::memory_order_relaxed)); }
    void setLabIdealMode(bool ideal) noexcept { setLabMode(ideal ? LabMode::Ideal : LabMode::Dna); }
    void setDriftFrozen(bool frozen) noexcept { labDriftFrozen.store(frozen, std::memory_order_relaxed); }
    bool driftFrozen() const noexcept { return labDriftFrozen.load(std::memory_order_relaxed); }

    // Diagnostic aggregates for the fingerprint/timeline display — written on
    // the audio thread as relaxed atomics, read from the UI timer.
    struct Diagnostics
    {
        std::atomic<float> supplySag { 0.0f };
        std::atomic<float> voiceLoad { 0.0f };
        std::atomic<float> warmupCents { 0.0f };
        std::atomic<float> unitDriftNorm { 0.0f };
    };
    const Diagnostics& diagnostics() const noexcept { return diag; }
    const VoiceManager& voices() const noexcept { return voiceManager; }

private:
    void applyEvent(const Event& event);
    void renderSpan(float* left, float* right, int numSamples);
    void updateControl(int numSamples);
    void processArp(int numSamples);
    void arpAddHeld(int note, float velocity);
    void arpRemoveHeld(int note);

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
    std::atomic<int> labMode { 1 };
    std::atomic<bool> labDriftFrozen { false };
    Diagnostics diag;
    float supplySag { 0.0f };  // shared circuit state (0 = fully recovered)

    // Arpeggiator state (audio-thread only, fixed capacity).
    struct HeldNote { int note; float velocity; };
    HeldNote arpHeld[16] {};
    int arpHeldCount { 0 };
    int arpStepIndex { 0 };
    int arpDirection { 1 };
    int arpSoundingNote { -1 };
    double arpStepCountdown { 0.0 };
    double arpGateCountdown { 0.0 };
    RandomSource arpRng;

    double sr { 48000.0 };
    QualityMode activeQuality { QualityMode::Normal };
    float outputDriveLinear { 1.0f };
    bool prepared { false };
};

} // namespace sappsynth
