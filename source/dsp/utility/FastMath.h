#pragma once
#include <cmath>
#include <algorithm>

namespace sappsynth {

inline constexpr double kPi    = 3.14159265358979323846;
inline constexpr double kTwoPi = 2.0 * kPi;

// Rational tanh approximation, accurate to ~1e-4 over the musical range and
// exactly bounded to [-1, 1]. Cheap enough for per-stage filter saturation.
inline float fastTanh(float x) noexcept
{
    x = std::clamp(x, -4.97f, 4.97f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float dbToGain(float db) noexcept   { return std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g)  noexcept   { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }

// Pitch in semitones relative to a reference note -> Hz. Modulation belongs in
// pitch space (semitones/cents) before this conversion — see architecture §8.1.
inline double noteToHz(double note, double referenceHz = 440.0, double referenceNote = 69.0) noexcept
{
    return referenceHz * std::exp2((note - referenceNote) / 12.0);
}

// Flush tiny values that would otherwise linger as denormals in feedback paths.
inline float flushDenormal(float x) noexcept
{
    return std::abs(x) < 1.0e-20f ? 0.0f : x;
}

} // namespace sappsynth
