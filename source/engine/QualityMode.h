#pragma once

namespace sappsynth {

enum class QualityMode { Eco, Normal, High };

// Prepared per-mode configuration (architecture §26): no `if (highQuality)`
// scattered through inner loops — components receive this once.
struct ProcessingQuality
{
    int oversamplingFactor { 2 };
    int filterSolverIterations { 2 };
    bool naiveOscillators { false };

    static ProcessingQuality forMode(QualityMode mode) noexcept
    {
        switch (mode)
        {
            case QualityMode::Eco:    return { 1, 1, false };
            case QualityMode::Normal: return { 2, 2, false };
            case QualityMode::High:   return { 4, 3, false };
        }
        return {};
    }
};

} // namespace sappsynth
