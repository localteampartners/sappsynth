#pragma once
#include <algorithm>
#include <cmath>
#include "BandLimitedOscillator.h"
#include "../utility/FastMath.h"

namespace sappsynth {

// VCO = ideal band-limited oscillator + layered pitch-error model
// (architecture §8.3/8.4): static offset, octave-tracking curvature, drift and
// note variation are supplied per control tick by the voice; this class turns
// them into a phase increment and renders.
class VCOModel
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        osc.reset();
    }

    void noteOn(double startPhase) noexcept { osc.reset(startPhase); }

    void setWaveform(Waveform w) noexcept          { osc.setWaveform(w); }
    void setMethod(OscillatorMethod m) noexcept    { osc.setMethod(m); }

    // Static per-voice-card errors, already scaled by the character amount.
    void setTolerances(float staticCents_, float trackErrorCentsPerOct) noexcept
    {
        staticCents = staticCents_;
        trackError = trackErrorCentsPerOct;
    }

    // note: final MIDI-style note number incl. patch octave/semi/fine and mods.
    // driftCents: correlated drift for this oscillator this control tick.
    void updatePitch(double note, float driftCents) noexcept
    {
        const double octavesFromCenter = (note - 60.0) / 12.0;
        const double errCents = static_cast<double>(staticCents)
                              + static_cast<double>(trackError) * octavesFromCenter
                              + static_cast<double>(driftCents);
        const double hz = noteToHz(note + errCents / 100.0);
        increment = std::clamp(hz / sr, 0.0, 0.45);
    }

    // fmMultiplier scales this sample's frequency (1 = no modulation);
    // clamped non-negative — no through-zero FM in this model.
    float tick(float pulseWidth, float fmMultiplier = 1.0f) noexcept
    {
        const double inc = std::clamp(increment * static_cast<double>(std::max(fmMultiplier, 0.0f)),
                                      0.0, 0.45);
        return osc.tick(inc, std::clamp(pulseWidth, 0.05f, 0.95f));
    }

    double phaseIncrement() const noexcept { return increment; }

private:
    BandLimitedOscillator osc;
    double sr { 48000.0 };
    double increment { 0.0 };
    float staticCents { 0.0f };
    float trackError { 0.0f };
};

} // namespace sappsynth
