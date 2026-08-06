#pragma once
#include <cmath>
#include "PhaseAccumulator.h"
#include "PolyBlep.h"
#include "../utility/FastMath.h"

namespace sappsynth {

enum class Waveform { Sine, Saw, Pulse, Triangle };

enum class OscillatorMethod { Naive, PolyBlep };

// One band-limited oscillator. Naive mode exists deliberately — Lab Mode's
// first experiment is "why naive saws alias" (architecture §20.1) and the
// alias-energy regression test compares the two renderers.
class BandLimitedOscillator
{
public:
    void reset(double startPhase = 0.0) noexcept
    {
        phase.reset(startPhase);
        triIntegrator = 0.0f;
    }

    void setMethod(OscillatorMethod m) noexcept { method = m; }
    void setWaveform(Waveform w) noexcept       { waveform = w; }

    // increment = frequency / sampleRate; pulseWidth in (0.05..0.95).
    float tick(double increment, float pulseWidth = 0.5f) noexcept
    {
        const double t = phase.value();
        float out = 0.0f;

        switch (waveform)
        {
            case Waveform::Sine:
                out = std::sin(static_cast<float>(t * kTwoPi));
                break;

            case Waveform::Saw:
                out = static_cast<float>(2.0 * t - 1.0);
                if (method == OscillatorMethod::PolyBlep)
                    out -= polyBlep(t, increment);
                break;

            case Waveform::Pulse:
                out = renderPulse(t, increment, pulseWidth);
                break;

            case Waveform::Triangle:
            {
                // Leaky integration of a band-limited square: triangle slope is
                // +-4/period, so gain = 4 * increment. Inherits the square's
                // alias suppression; the leak stops DC accumulating.
                const float square = renderPulse(t, increment, 0.5f);
                triIntegrator = 0.9995f * triIntegrator
                              + static_cast<float>(4.0 * increment) * square;
                out = triIntegrator;
                break;
            }
        }

        phase.advance(increment);
        return out;
    }

    double currentPhase() const noexcept { return phase.value(); }

private:
    float renderPulse(double t, double increment, float pulseWidth) noexcept
    {
        float out = t < static_cast<double>(pulseWidth) ? 1.0f : -1.0f;
        if (method == OscillatorMethod::PolyBlep)
        {
            out += polyBlep(t, increment);
            double t2 = t - static_cast<double>(pulseWidth);
            if (t2 < 0.0)
                t2 += 1.0;
            out -= polyBlep(t2, increment);
        }
        // Remove the DC that a non-square pulse otherwise carries.
        out -= 2.0f * pulseWidth - 1.0f;
        return out;
    }

    PhaseAccumulator phase;
    Waveform waveform { Waveform::Saw };
    OscillatorMethod method { OscillatorMethod::PolyBlep };
    float triIntegrator { 0.0f };
};

} // namespace sappsynth
