#pragma once
#include <array>
#include <cstdint>
#include "SynthVoice.h"

namespace sappsynth {

// Fixed pool of 16 voice cards (architecture §15.2). Round-robin allocation so
// repeated notes land on different cards; steals the oldest voice (preferring
// releasing ones) when the patch polyphony is exhausted.
class VoiceManager
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare(double sampleRate);
    void setUnitSeed(std::uint64_t unitSeed);
    void applyQuality(const ProcessingQuality& quality);

    // Silences every card and parks the round-robin cursor on `startCard`.
    // Which cards a chord lands on is audible: each card carries its own gain
    // and pan tolerances, so the same chord measures up to 3.5 dB apart
    // depending on where the cursor happens to be. Measurement code needs to
    // pin it (and to sweep it) rather than inherit whatever the last note left.
    void resetAllocation(int startCard = 0) noexcept;

    // glideFromNote: previous note for portamento, -1 for none.
    void noteOn(int note, float velocity, const PatchState& patch, const UnitProfile& unit,
                int glideFromNote = -1);
    void noteOff(int note);
    void allNotesOff();

    int activeVoiceCount() const noexcept;

    template <typename Fn>
    void forEachActiveVoice(Fn&& fn)
    {
        for (auto& voice : voices)
            if (voice.isActive())
                fn(voice);
    }

    SynthVoice& voice(int index) noexcept { return voices[static_cast<std::size_t>(index)]; }
    const SynthVoice& voice(int index) const noexcept { return voices[static_cast<std::size_t>(index)]; }

private:
    SynthVoice* findFreeVoice() noexcept;
    SynthVoice* findStealTarget() noexcept;

    std::array<SynthVoice, kMaxVoices> voices {};
    std::uint64_t unitSeed_ { 0 };
    std::uint64_t allocationCounter { 0 };
    std::uint64_t noteCounter { 0 };
    int roundRobinCursor { 0 };
};

} // namespace sappsynth
