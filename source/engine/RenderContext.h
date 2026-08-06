#pragma once
#include <cstdint>
#include <span>

namespace sappsynth {

// Sample-accurate events (architecture §6.3). The engine splits each host
// block into spans at event boundaries; events must be sorted by sampleOffset.
struct Event
{
    enum class Type { NoteOn, NoteOff, AllNotesOff };

    Type type { Type::NoteOn };
    int sampleOffset { 0 };
    int note { 60 };
    float velocity { 1.0f };
};

struct RenderBlock
{
    float* left { nullptr };
    float* right { nullptr };
    int numSamples { 0 };
    std::span<const Event> events {};
};

} // namespace sappsynth
