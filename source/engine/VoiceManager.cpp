#include "VoiceManager.h"

namespace sappsynth {

void VoiceManager::prepare(double sampleRate)
{
    int index = 0;
    for (auto& voice : voices)
    {
        voice.prepare(sampleRate);
        voice.configureIdentity(index++, unitSeed_);
    }
    allocationCounter = 0;
    noteCounter = 0;
    roundRobinCursor = 0;
}

void VoiceManager::setUnitSeed(std::uint64_t unitSeed)
{
    unitSeed_ = unitSeed;
    int index = 0;
    for (auto& voice : voices)
        voice.configureIdentity(index++, unitSeed_);
    noteCounter = 0;
}

void VoiceManager::applyQuality(const ProcessingQuality& quality)
{
    for (auto& voice : voices)
        voice.applyQuality(quality);
}

void VoiceManager::resetAllocation(int startCard) noexcept
{
    for (auto& voice : voices)
        voice.hardStop();
    allocationCounter = 0;
    noteCounter = 0;
    roundRobinCursor = ((startCard % kMaxVoices) + kMaxVoices) % kMaxVoices;
}

int VoiceManager::activeVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& voice : voices)
        if (voice.isActive())
            ++count;
    return count;
}

SynthVoice* VoiceManager::findFreeVoice() noexcept
{
    // Round-robin over the card pool: repeated notes get different circuits.
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const int idx = (roundRobinCursor + i) % kMaxVoices;
        if (!voices[static_cast<std::size_t>(idx)].isActive())
        {
            roundRobinCursor = (idx + 1) % kMaxVoices;
            return &voices[static_cast<std::size_t>(idx)];
        }
    }
    return nullptr;
}

SynthVoice* VoiceManager::findStealTarget() noexcept
{
    SynthVoice* best = nullptr;
    // Prefer the oldest releasing voice; otherwise the oldest *active* voice.
    // Inactive voices are never steal targets — stealing one would grow the
    // active count past the polyphony limit.
    for (auto& voice : voices)
        if (voice.isActive() && voice.isReleasing()
            && (best == nullptr || voice.allocationAge() < best->allocationAge()))
            best = &voice;
    if (best != nullptr)
        return best;
    for (auto& voice : voices)
        if (voice.isActive() && (best == nullptr || voice.allocationAge() < best->allocationAge()))
            best = &voice;
    return best;
}

void VoiceManager::noteOn(int note, float velocity, const PatchState& patch, const UnitProfile& unit,
                          int glideFromNote)
{
    // One note = one voice card. Unison members are rendered INSIDE the card
    // (SynthVoice's stack) since v0.3; before, each member took a card of its
    // own, so a 5-voice unison lead on an 8-voice patch was mono and a
    // supersaw pad stole its own notes.
    const int polyphony = std::clamp(patch.polyphony, 1, kMaxVoices);
    SynthVoice* target = activeVoiceCount() < polyphony ? findFreeVoice() : nullptr;
    if (target == nullptr)
        target = findStealTarget();
    if (target == nullptr)
        return;

    SynthVoice::NoteStart start;
    start.unisonDetuneCents = 0.0f;
    start.unisonPan = 0.0f;
    start.unisonGain = 1.0f;
    start.glideFromNote = glideFromNote;

    target->setAllocationAge(allocationCounter++);
    target->noteOn(note, velocity, seeds::noteSeed(unitSeed_, noteCounter++), patch, unit, start);
}

void VoiceManager::noteOff(int note)
{
    for (auto& voice : voices)
        if (voice.isActive() && voice.currentNote() == note && !voice.isReleasing())
            voice.noteOff();
}

void VoiceManager::allNotesOff()
{
    for (auto& voice : voices)
        if (voice.isActive())
            voice.noteOff();
}

} // namespace sappsynth
