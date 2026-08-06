#pragma once
#include "../utility/FastMath.h"

namespace sappsynth {

// Soft saturation with controllable asymmetry. Asymmetry introduces even
// harmonics the way a slightly biased circuit stage does. The DC that bias
// creates is compensated by the caller (MixerModel / VcaModel).
inline float saturate(float x, float drive, float asymmetry) noexcept
{
    const float driven = x * drive + asymmetry;
    return fastTanh(driven);
}

// Static DC offset produced by `saturate` for silent input — subtract this so
// nonlinear stages do not leak DC into the filter.
inline float saturationDcOffset(float drive, float asymmetry) noexcept
{
    (void) drive;
    return fastTanh(asymmetry);
}

} // namespace sappsynth
