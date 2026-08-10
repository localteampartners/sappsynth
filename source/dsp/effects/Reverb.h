#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../utility/FastMath.h"

namespace sappsynth {

// Clean algorithmic reverb (architecture §22 says the first release does not
// need circuit modeling here): Schroeder/Freeverb topology — 8 damped combs +
// 4 allpasses per channel, right channel offset for stereo width.
class Reverb
{
public:
    void prepare(double sampleRate) noexcept
    {
        const double scale = sampleRate / 44100.0;
        static constexpr int combTunings[8] { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static constexpr int allpassTunings[4] { 556, 441, 341, 225 };
        static constexpr int stereoSpread = 23;

        for (int ch = 0; ch < 2; ++ch)
        {
            for (int c = 0; c < 8; ++c)
            {
                auto& comb = combs[ch][c];
                comb.buffer.assign(static_cast<std::size_t>(
                    static_cast<int>(combTunings[c] * scale) + ch * stereoSpread), 0.0f);
                comb.index = 0;
                comb.filterState = 0.0f;
            }
            for (int a = 0; a < 4; ++a)
            {
                auto& ap = allpasses[ch][a];
                ap.buffer.assign(static_cast<std::size_t>(
                    static_cast<int>(allpassTunings[a] * scale) + ch * stereoSpread), 0.0f);
                ap.index = 0;
            }
        }
    }

    void setParameters(float size, float mixAmount) noexcept
    {
        roomFeedback = 0.70f + 0.28f * std::clamp(size, 0.0f, 1.0f);
        damping = 0.4f;
        mix = std::clamp(mixAmount, 0.0f, 1.0f);
        wetFeed = kWetReference / std::sqrt(combBankPowerGain(roomFeedback, damping));
    }

    void process(float* left, float* right, int n) noexcept
    {
        if (mix <= 0.0001f)
            return;

        for (int i = 0; i < n; ++i)
        {
            const float input = (left[i] + right[i]) * 0.5f * wetFeed;
            float outs[2] { 0.0f, 0.0f };
            for (int ch = 0; ch < 2; ++ch)
            {
                float acc = 0.0f;
                for (auto& comb : combs[ch])
                    acc += comb.process(input, roomFeedback, damping);
                for (auto& ap : allpasses[ch])
                    acc = ap.process(acc);
                outs[ch] = acc;
            }
            // Crossfade, not a boost (issue #2, fault 2): mix is a balance
            // between dry and wet, so raising it can never raise the level.
            left[i] += mix * (outs[0] - left[i]);
            right[i] += mix * (outs[1] - right[i]);
        }
    }

    // Broadband power gain of ONE damped comb at feedback f and damping d
    // (issue #2, fault 1). The wet path used to be fed at a fixed 0.25 where
    // Freeverb uses 0.015, so eight parallel combs put the wet signal 10-30 dB
    // above the dry one and `Size` — which only moves f — worked as a loudness
    // control.
    //
    // Averaging 1/(1 - g(w)^2) over the comb's peaks and notches, with
    // g(w) = f * |H_damp(w)| and H_damp the one-pole in the feedback path,
    // integrates in closed form:
    //
    //     A = 1 + d^2 - f^2 (1-d)^2
    //     P = 1 + f^2 (1-d)^2 / sqrt(A^2 - 4d^2)
    //
    // Dividing the feed by sqrt(P) holds the wet level put while f — and so
    // the tail length — still runs from 0.70 to 0.98. Damping matters as much
    // as feedback here: it is why the naive 1/sqrt(1-f^2) normalisation
    // over-corrects the top of the Size range by ~7 dB.
    static float combBankPowerGain(float f, float d) noexcept
    {
        const float lossless = (1.0f - d) * (1.0f - d) * f * f;
        const float a = 1.0f + d * d - lossless;
        const float root = std::sqrt(std::max(a * a - 4.0f * d * d, 1.0e-6f));
        return 1.0f + lossless / root;
    }

private:
    struct Comb
    {
        std::vector<float> buffer;
        std::size_t index { 0 };
        float filterState { 0.0f };

        float process(float x, float feedback, float damp) noexcept
        {
            const float out = buffer[index];
            filterState = flushDenormal(out * (1.0f - damp) + filterState * damp);
            buffer[index] = x + filterState * feedback;
            if (++index >= buffer.size())
                index = 0;
            return out;
        }
    };

    struct Allpass
    {
        std::vector<float> buffer;
        std::size_t index { 0 };

        float process(float x) noexcept
        {
            const float delayed = buffer[index];
            const float out = delayed - x;
            buffer[index] = flushDenormal(x + delayed * 0.5f);
            if (++index >= buffer.size())
                index = 0;
            return out;
        }
    };

    // Trim that lands the normalised wet path at the dry signal's broadband
    // RMS (0.0 dB) at full wet. It absorbs the two constant gains the model
    // above leaves out: eight combs summing incoherently (sqrt(8)) and the
    // four Schroeder sections, which are not unity-gain in this form — each
    // one is +1.8 dB broadband, +7.3 dB over the chain. Measured, not guessed:
    // see tests/unit/test_gainstaging.cpp.
    static constexpr float kWetReference = 0.0655f;

    Comb combs[2][8];
    Allpass allpasses[2][4];
    float roomFeedback { 0.85f };
    float damping { 0.4f };
    float mix { 0.0f };
    float wetFeed { kWetReference };
};

} // namespace sappsynth
