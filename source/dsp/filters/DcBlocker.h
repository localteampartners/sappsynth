#pragma once
#include "../utility/FastMath.h"

namespace sappsynth {

// One-pole DC blocker — the "HP" block between mixer and filter in the signal
// path (architecture §7). Asymmetric saturation rectifies signal-dependent DC
// that static compensation cannot remove; this is what actually removes it.
class DcBlocker
{
public:
    void prepare(double sampleRate, float cutoffHz = 8.0f) noexcept
    {
        R = 1.0f - static_cast<float>(kTwoPi * cutoffHz / sampleRate);
        reset();
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    float process(float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = flushDenormal(y);
        return y;
    }

private:
    float R { 0.999f };
    float x1 { 0.0f };
    float y1 { 0.0f };
};

} // namespace sappsynth
