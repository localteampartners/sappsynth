#pragma once
#include <cmath>
#include "RandomSource.h"

namespace sappsynth {

// Ornstein-Uhlenbeck drift (architecture §9.2). White noise on pitch sounds
// like fuzz; OU gives smooth, bounded wander. Updated at a low control rate
// (default 50 Hz) and linearly interpolated between updates.
class DriftProcess
{
public:
    void prepare(double sampleRate, float updateRateHz = 50.0f) noexcept
    {
        updateIntervalSamples = static_cast<int>(sampleRate / updateRateHz);
        if (updateIntervalSamples < 1)
            updateIntervalSamples = 1;
        dt = 1.0f / updateRateHz;
        countdown = 0;
        current = previous = next_ = 0.0f;
    }

    void seed(std::uint64_t seedValue) noexcept
    {
        rng.seed(seedValue);
        current = previous = next_ = 0.0f;
        countdown = 0;
    }

    // theta: mean-reversion rate (1/s). sigma: intensity. Stationary std-dev
    // is sigma / sqrt(2*theta) — callers scale output to cents/Hz/etc.
    void setParameters(float meanReversion, float intensity) noexcept
    {
        theta = meanReversion;
        sigma = intensity;
    }

    // Advance by `numSamples` audio samples; returns the interpolated value at
    // the *start* of the span. Cheap: stochastic step only every update tick.
    float process(int numSamples) noexcept
    {
        if (countdown <= 0)
        {
            previous = next_;
            const float dx = theta * (0.0f - next_) * dt
                           + sigma * std::sqrt(dt) * rng.normal();
            next_ += dx;
            countdown = updateIntervalSamples;
        }
        const float phase = 1.0f - static_cast<float>(countdown) / static_cast<float>(updateIntervalSamples);
        current = previous + (next_ - previous) * phase;
        countdown -= numSamples;
        return current;
    }

    float value() const noexcept { return current; }

private:
    RandomSource rng;
    float theta { 0.5f };
    float sigma { 1.0f };
    float dt { 0.02f };
    float previous { 0.0f };
    float next_ { 0.0f };
    float current { 0.0f };
    int updateIntervalSamples { 960 };
    int countdown { 0 };
};

} // namespace sappsynth
