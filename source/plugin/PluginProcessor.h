#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>
#include "../engine/SynthEngine.h"

namespace sappsynth {

// Thin plugin adapter (architecture §4): translates host concepts — APVTS
// parameters, MidiBuffer, state blobs — into engine concepts. No sound-design
// logic lives here.
class SappSynthProcessor : public juce::AudioProcessor
{
public:
    SappSynthProcessor();

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

    int getNumPrograms() override                 { return 1; }
    int getCurrentProgram() override              { return 0; }
    void setCurrentProgram(int) override          {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappSynthProcessor)
};

} // namespace sappsynth
