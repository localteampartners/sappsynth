#pragma once
#include <cmath>
#include "../utility/FastMath.h"
#include "../oscillators/PhaseAccumulator.h"

namespace sappsynth {

enum class LfoShape { Sine, Triangle, Saw, Square };

// Control-rate LFO. Evaluated once per control tick; destinations interpolate
// implicitly through their own smoothing.
class Lfo
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        phase.reset();
    }

    void reset(double startPhase = 0.0) noexcept { phase.reset(startPhase); }
    void setShape(LfoShape s) noexcept { shape = s; }
    void setRate(float hz) noexcept { rateHz = hz; }

    // Advance by numSamples audio samples, return value in [-1, 1].
    float process(int numSamples) noexcept
    {
        const double t = phase.value();
        phase.advance(static_cast<double>(rateHz) / sr * numSamples);
        switch (shape)
        {
            case LfoShape::Sine:     return std::sin(static_cast<float>(t * kTwoPi));
            case LfoShape::Triangle: return t < 0.5 ? static_cast<float>(4.0 * t - 1.0)
                                                    : static_cast<float>(3.0 - 4.0 * t);
            case LfoShape::Saw:      return static_cast<float>(2.0 * t - 1.0);
            case LfoShape::Square:   return t < 0.5 ? 1.0f : -1.0f;
        }
        return 0.0f;
    }

private:
    PhaseAccumulator phase;
    double sr { 48000.0 };
    float rateHz { 1.0f };
    LfoShape shape { LfoShape::Sine };
};

} // namespace sappsynth
