#pragma once

namespace sappsynth {

// High-resolution phase in [0, 1) (architecture §8.1). Double precision keeps
// pitch error far below a cent even after hours at 20 kHz.
class PhaseAccumulator
{
public:
    void reset(double startPhase = 0.0) noexcept
    {
        phase = startPhase - static_cast<double>(static_cast<long long>(startPhase));
        if (phase < 0.0)
            phase += 1.0;
    }

    // Advance by increment (= frequency / sampleRate). Returns true on wrap.
    bool advance(double increment) noexcept
    {
        phase += increment;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            return true;
        }
        return false;
    }

    double value() const noexcept { return phase; }

private:
    double phase { 0.0 };
};

} // namespace sappsynth
