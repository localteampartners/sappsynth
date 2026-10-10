#pragma once
#include <algorithm>

namespace sappsynth {

// TEMPO SYNC (v0.12, plan E3). The synth never read the host's BPM: the arp
// ran in Hz, the delay in seconds, the LFO in Hz — so nothing it did could
// land on the grid of the song it was in. A sync choice of 0 keeps the free
// rate; 1..9 pick a note division of the host tempo.
//
//   0 free   1 1/1   2 1/2   3 1/4   4 1/8   5 1/16   6 1/4T   7 1/8T   8 1/4D   9 1/8D
inline constexpr int kSyncChoices = 10;
inline constexpr const char* kSyncNames[kSyncChoices] {
    "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4D", "1/8D"
};

/** Length of one division in beats (quarter notes); 0 for "free". */
inline float syncDivisionBeats(int choice) noexcept
{
    static constexpr float beats[kSyncChoices] { 0.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 2.0f / 3.0f, 1.0f / 3.0f, 1.5f, 0.75f };
    return beats[std::clamp(choice, 0, kSyncChoices - 1)];
}

/** Seconds per division at a tempo. */
inline float syncSeconds(int choice, double bpm) noexcept
{
    const float beats = syncDivisionBeats(choice);
    return beats <= 0.0f ? 0.0f : beats * static_cast<float>(60.0 / std::max(20.0, bpm));
}

/** Cycles per second for a division (one cycle per division). */
inline float syncHz(int choice, double bpm) noexcept
{
    const float s = syncSeconds(choice, bpm);
    return s <= 0.0f ? 0.0f : 1.0f / s;
}

} // namespace sappsynth
