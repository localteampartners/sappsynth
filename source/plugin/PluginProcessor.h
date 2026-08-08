#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <vector>
#include "../engine/SynthEngine.h"
#include "UserPresets.h"

namespace sappsynth {

// Thin plugin adapter (architecture §4): translates host concepts — APVTS
// parameters, MidiBuffer, state blobs — into engine concepts. No sound-design
// logic lives here.
class SappSynthProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    // The SappLink instrument name: names the user-preset folder and must
    // match sapplink/manifests/sappsynth.json.
    static constexpr const char* kInstrument = "sappsynth";

    SappSynthProcessor();
    ~SappSynthProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override               { return true; }

    const juce::String getName() const override   { return "SappSynth"; }
    bool acceptsMidi() const override             { return true; }
    bool producesMidi() const override            { return false; }
    bool isMidiEffect() const override            { return false; }
    double getTailLengthSeconds() const override  { return 2.0; }

    // Factory-preset programs: program N is presets::all()[N]. Selectable
    // from the host program API and via MIDI program change (SappLink
    // set_patches). Presets are starting points — CCs keep working on top.
    int getNumPrograms() override;
    int getCurrentProgram() override              { return currentProgram.load(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    // Apply factory preset N now. Message thread only (resets every
    // parameter to its default, then applies the preset's values).
    void applyFactoryPreset(int index);

    // ---------------------------------------------------------- user presets --
    // Saved sounds, shared format across the suite (sapplink/PRESETS.md).
    // Factory presets stay addressed by program index; user presets are
    // addressed by NAME, so the two can never collide.

    // Capture the current parameter state to
    // <Documents>/SappSounds/presets/sappsynth/<name>.json. Message thread.
    bool saveUserPreset(const juce::String& name, const juce::String& notes, juce::String& error);

    // Load a user preset by name (case-insensitive). Message thread.
    bool loadUserPreset(const juce::String& name, juce::String& error);

    // Fresh scan of the user preset folder.
    std::vector<sapp::userpresets::UserPreset> userPresets() const;

    // Choice-list geometry of the `preset` parameter: [0, factoryPresetCount)
    // are factory programs, the rest are the user presets discovered when this
    // instance was constructed.
    int factoryPresetCount() const;

    // Apply choice N of the `preset` parameter. Message thread.
    void applyPresetChoice(int index);

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Reroll the virtual instrument (new unit seed / new "physical unit").
    // Locking pins the unit identity (dna.md: seed must survive edits).
    void rerollUnitSeed();
    juce::String unitSeedText() const;
    bool isSeedLocked() const noexcept { return seedLocked; }
    void setSeedLocked(bool locked) noexcept { seedLocked = locked; }

    // Lab view access (telemetry tap + A/B flags live on the engine).
    SynthEngine& synthEngine() noexcept { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    PatchState buildPatchFromParameters();

    SynthEngine engine;
    std::vector<Event> eventScratch;
    bool seedLocked { false };

    // SappLink CC-in slew: CC steps land as targets; each block moves the
    // parameter a fraction of the way so 7-bit steps don't zipper. Parameter
    // pointers resolve once in the constructor (audio thread stays lookup-free).
    struct CcSlew
    {
        juce::RangedAudioParameter* parameter { nullptr };
        float target { 0.0f };
        float current { 0.0f };
        bool active { false };
    };
    std::array<CcSlew, 20> ccSlews;
    void handleSappLinkCc(int ccNumber, int ccValue);
    void advanceCcSlews(int numSamples, double sampleRate);

    // MIDI program change lands on the audio thread; the preset itself is
    // applied on the message thread (timer), sappstep-style.
    void timerCallback() override;
    std::atomic<int> pendingProgram { -1 };
    std::atomic<int> currentProgram { 0 };

    // The `preset` parameter can be moved from the audio thread (host
    // automation), so its listener only stores an index — the same timer that
    // already defers program changes does the loading.
    void parameterChanged(const juce::String& parameterId, float newValue) override;
    std::atomic<int> pendingPresetChoice { -1 };
    // Set while WE are moving the `preset` parameter, so syncing it after a
    // program change never re-enters the load.
    bool applyingPreset { false };
    void syncPresetParameter(int choiceIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappSynthProcessor)
};

} // namespace sappsynth
