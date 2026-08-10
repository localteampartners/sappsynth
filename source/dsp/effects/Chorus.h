#pragma once
#include <cmath>
#include <vector>
#include "../utility/FastMath.h"

namespace sappsynth {

// Analog-style chorus (architecture §22): three modulated taps per channel,
// slightly different rates, stereo LFO phase offsets, bandwidth-limited wet
// path. Subtle by default. Buffers allocate in prepare() only.
class Chorus
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        const int size = nextPow2(static_cast<int>(sr * 0.06) + 4); // 60 ms
        mask = size - 1;
        for (auto& line : lines)
        {
            line.assign(static_cast<std::size_t>(size), 0.0f);
        }
        writeIndex = 0;
        for (auto& p : phase)
            p = 0.0;
        for (auto& s : lpState)
            s = 0.0f;
    }

    void setParameters(float mixAmount, float rateHz) noexcept
    {
        mix = std::clamp(mixAmount, 0.0f, 1.0f);
        rate = std::clamp(rateHz, 0.02f, 5.0f);
    }

    void process(float* left, float* right, int n) noexcept
    {
        if (mix <= 0.0001f)
            return;

        const float baseDelay = static_cast<float>(sr) * 0.014f;
        const float depth = static_cast<float>(sr) * 0.0045f;
        const float lpCoef = 1.0f - std::exp(-static_cast<float>(kTwoPi) * 7000.0f / static_cast<float>(sr));
        // Slight rate variation per tap — real BBD choruses never line up.
        const double inc0 = rate / sr, inc1 = rate * 1.13 / sr, inc2 = rate * 0.87 / sr;

        for (int i = 0; i < n; ++i)
        {
            lines[0][static_cast<std::size_t>(writeIndex)] = left[i];
            lines[1][static_cast<std::size_t>(writeIndex)] = right[i];

            auto tap = [&](int channel, double lfoPhase, double phaseOffset) noexcept
            {
                const float lfo = std::sin(static_cast<float>((lfoPhase + phaseOffset) * kTwoPi));
                const float delay = baseDelay + depth * (1.0f + lfo) * 0.5f;
                const float readPos = static_cast<float>(writeIndex) - delay;
                const int i0 = static_cast<int>(std::floor(readPos));
                const float frac = readPos - static_cast<float>(i0);
                const auto& line = lines[static_cast<std::size_t>(channel)];
                const float a = line[static_cast<std::size_t>(i0 & mask)];
                const float b = line[static_cast<std::size_t>((i0 + 1) & mask)];
                return a + (b - a) * frac;
            };

            float wetL = tap(0, phase[0], 0.0) + tap(0, phase[1], 0.33);
            float wetR = tap(1, phase[1], 0.5) + tap(1, phase[2], 0.83);

            // Bandwidth-limit the wet signal (BBD-style darkening).
            lpState[0] += lpCoef * (wetL * 0.5f - lpState[0]);
            lpState[1] += lpCoef * (wetR * 0.5f - lpState[1]);

            // Crossfade, not a boost (issue #2, fault 2). The old form kept the
            // dry signal at full level and added the wet on top, so Mix was a
            // level control with a chorus attached.
            left[i] += mix * (lpState[0] - left[i]);
            right[i] += mix * (lpState[1] - right[i]);

            writeIndex = (writeIndex + 1) & mask;
            phase[0] += inc0; phase[1] += inc1; phase[2] += inc2;
            for (auto& p : phase)
                if (p >= 1.0)
                    p -= 1.0;
        }
    }

private:
    static int nextPow2(int v) noexcept { int p = 1; while (p < v) p <<= 1; return p; }

    std::vector<float> lines[2];
    double phase[3] { 0.0, 0.25, 0.5 };
    float lpState[2] { 0.0f, 0.0f };
    double sr { 48000.0 };
    int writeIndex { 0 };
    int mask { 0 };
    float mix { 0.0f };
    float rate { 0.5f };
};

} // namespace sappsynth
