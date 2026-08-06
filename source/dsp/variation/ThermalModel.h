#pragma once
#include <cmath>

namespace sappsynth {

// warmupError(t) = initialError * exp(-t / timeConstant)  (architecture §9.3).
// Default experience starts nearly warmed up; Lab Mode can exaggerate.
class ThermalModel
{
public:
    void prepare(double sampleRate) noexcept
    {
        sampleRateHz = sampleRate;
        elapsedSeconds = startSeconds;
    }

    // amount 0..1 scales initial error; timeConstant in seconds.
    void configure(float initialErrorCents, float timeConstantSeconds, float startWarmedFraction = 0.9f) noexcept
    {
        initialCents = initialErrorCents;
        tau = timeConstantSeconds > 0.1f ? timeConstantSeconds : 0.1f;
        // startWarmedFraction 0.9 => begin at the point where 90% of the error
        // has already decayed away.
        startSeconds = -tau * std::log(1.0f - startWarmedFraction);
        elapsedSeconds = startSeconds;
    }

    void coldStart() noexcept { elapsedSeconds = 0.0f; }

    float advance(int numSamples) noexcept
    {
        elapsedSeconds += static_cast<float>(numSamples / sampleRateHz);
        return currentCents();
    }

    float currentCents() const noexcept
    {
        return initialCents * std::exp(-elapsedSeconds / tau);
    }

private:
    double sampleRateHz { 48000.0 };
    float initialCents { 8.0f };
    float tau { 90.0f };
    float startSeconds { 0.0f };
    float elapsedSeconds { 0.0f };
};

} // namespace sappsynth
