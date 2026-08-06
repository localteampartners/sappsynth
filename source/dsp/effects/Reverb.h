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
    }

    void process(float* left, float* right, int n) noexcept
    {
        if (mix <= 0.0001f)
            return;

        for (int i = 0; i < n; ++i)
        {
            const float input = (left[i] + right[i]) * 0.25f;
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
            left[i] += mix * (outs[0] - left[i] * 0.3f * mix);
            right[i] += mix * (outs[1] - right[i] * 0.3f * mix);
        }
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

    Comb combs[2][8];
    Allpass allpasses[2][4];
    float roomFeedback { 0.85f };
    float damping { 0.4f };
    float mix { 0.0f };
};

} // namespace sappsynth
