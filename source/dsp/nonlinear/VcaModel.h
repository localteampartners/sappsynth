#pragma once
#include "Saturators.h"

namespace sappsynth {

// Modeled VCA (architecture §14): drive -> asymmetric soft saturation ->
// curved envelope gain. The curve makes low envelope values fade musically
// instead of linearly; per-card gain bias and asymmetry come from the profile.
class VcaModel
{
public:
    void configure(float driveAmount, float asymmetry_, float gainBiasLinear) noexcept
    {
        drive = 1.0f + driveAmount;
        asymmetry = asymmetry_;
        gainBias = gainBiasLinear;
        dcCompensation = saturationDcOffset(drive, asymmetry);
    }

    float process(float x, float envelopeGain) noexcept
    {
        const float shaped = (saturate(x, drive, asymmetry) - dcCompensation) / drive;
        // Perceptual gain curve: ~x^1.6 between linear and exponential.
        const float g = envelopeGain * envelopeGain
                      / (envelopeGain + 0.6f * (1.0f - envelopeGain) + 1.0e-9f);
        return shaped * g * gainBias;
    }

private:
    float drive { 1.0f };
    float asymmetry { 0.0f };
    float gainBias { 1.0f };
    float dcCompensation { 0.0f };
};

} // namespace sappsynth
