#pragma once
#include <algorithm>
#include <cmath>
#include "../utility/FastMath.h"

namespace sappsynth {

// The output ceiling (issue #2, fault 3).
//
// The old stage was `fastTanh(x * drive * 0.8) / fastTanh(max(drive,1) * 0.8)`.
// That normalises the curve at FULL SCALE, so everything below full scale came
// out with the small-signal gain of the curve — 0.8 * 1.506 = +1.6 dB — while
// the comment claimed it was transparent until it limited. Unity was not unity.
//
// This is a soft knee instead: bit-exact identity below kOutputKnee, C1
// continuous at the knee (the tanh's derivative at 0 is exactly 1), and
// asymptotic to full scale above it. Nothing is applied to a signal that does
// not need limiting.
inline constexpr float kOutputKnee = 0.5f;   // -6.02 dBFS

inline float softKnee(float x) noexcept
{
    const float magnitude = std::abs(x);
    if (magnitude <= kOutputKnee)
        return x;

    const float over = (magnitude - kOutputKnee) / (1.0f - kOutputKnee);
    // fastTanh overshoots 1.0 slightly at its clamp; the min keeps the ceiling
    // a real ceiling.
    const float limited = kOutputKnee + (1.0f - kOutputKnee) * std::min(fastTanh(over), 1.0f);
    return x < 0.0f ? -limited : limited;
}

// Output drive: an honest gain into the knee. At driveLinear == 1 this is the
// identity for anything below the knee — unity is unity.
inline float outputStage(float x, float driveLinear) noexcept
{
    return softKnee(x * driveLinear);
}

} // namespace sappsynth
