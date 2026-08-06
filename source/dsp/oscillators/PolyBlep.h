#pragma once

namespace sappsynth {

// Two-sample polynomial band-limited step residual. Added at waveform
// discontinuities to cancel the worst aliasing of naive saw/pulse waves.
// t: phase in [0,1). dt: phase increment per sample.
inline float polyBlep(double t, double dt) noexcept
{
    if (t < dt)                       // just after the discontinuity
    {
        const double x = t / dt;
        return static_cast<float>(x + x - x * x - 1.0);
    }
    if (t > 1.0 - dt)                 // just before the discontinuity
    {
        const double x = (t - 1.0) / dt;
        return static_cast<float>(x * x + x + x + 1.0);
    }
    return 0.0f;
}

} // namespace sappsynth
