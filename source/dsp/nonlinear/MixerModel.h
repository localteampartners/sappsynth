#pragma once
#include "Saturators.h"

namespace sappsynth {

// Nonlinear source mixer (architecture §10). Sits *before* the filter: input
// level into the ladder is a first-class part of the sound, so oscillator
// levels interact instead of just summing. Gain-staged so one oscillator at
// full level hits the saturator at a known reference amplitude.
class MixerModel
{
public:
    void setGains(float osc1, float osc2, float sub, float noise) noexcept
    {
        g1 = osc1; g2 = osc2; gs = sub; gn = noise;
    }

    // drive >= 1 pushes harder into saturation; character adds asymmetry.
    void setCharacter(float drive_, float asymmetry_) noexcept
    {
        drive = drive_;
        asymmetry = asymmetry_;
        dcCompensation = saturationDcOffset(drive, asymmetry);
    }

    // The two halves are separable so the linear sum can be computed at base
    // rate while the saturation runs inside the oversampled island.
    float weightedSum(float osc1, float osc2, float sub, float noise) const noexcept
    {
        // 0.5 reference: one osc at unity gain = -6 dBFS into the saturator,
        // leaving headroom that shrinks audibly as more sources are added.
        return 0.5f * (g1 * osc1 + g2 * osc2 + gs * sub + gn * noise);
    }

    float saturateSum(float summed) const noexcept
    {
        return saturate(summed, drive, asymmetry) - dcCompensation;
    }

    float process(float osc1, float osc2, float sub, float noise) const noexcept
    {
        return saturateSum(weightedSum(osc1, osc2, sub, noise));
    }

private:
    float g1 { 1.0f }, g2 { 0.0f }, gs { 0.0f }, gn { 0.0f };
    float drive { 1.0f };
    float asymmetry { 0.0f };
    float dcCompensation { 0.0f };
};

} // namespace sappsynth
