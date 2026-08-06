#pragma once
#include <cmath>

namespace sappsynth {

// One-pole exponential parameter smoother. Different parameters want different
// time constants (architecture §17.3); the owner picks the time at prepare().
class SmoothedValue
{
public:
    void prepare(double sampleRate, float smoothingSeconds) noexcept
    {
        coefficient = smoothingSeconds > 0.0f
            ? static_cast<float>(std::exp(-1.0 / (smoothingSeconds * sampleRate)))
            : 0.0f;
    }

    void reset(float value) noexcept        { current = target = value; }
    void setTarget(float value) noexcept    { target = value; }

    float next() noexcept
    {
        current = target + coefficient * (current - target);
        return current;
    }

    float value() const noexcept            { return current; }
    float targetValue() const noexcept      { return target; }

private:
    float current { 0.0f };
    float target { 0.0f };
    float coefficient { 0.0f };
};

} // namespace sappsynth
