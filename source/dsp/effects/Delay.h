#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "../utility/FastMath.h"

namespace sappsynth {

// Stereo delay with tape-ish darkening in the feedback loop and a light
// ping-pong cross-feed. Time changes are smoothed to avoid pitch zips.
class StereoDelay
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        const int size = nextPow2(static_cast<int>(sr * 1.6) + 4);
        mask = size - 1;
        for (auto& line : lines)
            line.assign(static_cast<std::size_t>(size), 0.0f);
        writeIndex = 0;
        smoothedTime = 0.35f * static_cast<float>(sr);
        lpState[0] = lpState[1] = 0.0f;
    }

    // Nominal peak level of the bus feeding this delay. The feedback path's
    // tape saturation is scaled to it, so lowering the bus for headroom does
    // not quietly linearise the echoes.
    void setLevelReference(float reference) noexcept
    {
        levelReference = std::max(reference, 1.0e-3f);
    }

    void setParameters(float timeSeconds, float feedbackAmount, float mixAmount) noexcept
    {
        targetTime = std::clamp(timeSeconds, 0.03f, 1.5f) * static_cast<float>(sr);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.9f);
        mix = std::clamp(mixAmount, 0.0f, 1.0f);
    }

    void process(float* left, float* right, int n) noexcept
    {
        if (mix <= 0.0001f)
        {
            // Keep writing so the line stays warm for re-enable.
            for (int i = 0; i < n; ++i)
            {
                lines[0][static_cast<std::size_t>(writeIndex)] = left[i];
                lines[1][static_cast<std::size_t>(writeIndex)] = right[i];
                writeIndex = (writeIndex + 1) & mask;
            }
            return;
        }

        const float lpCoef = 1.0f - std::exp(-static_cast<float>(kTwoPi) * 5000.0f / static_cast<float>(sr));
        for (int i = 0; i < n; ++i)
        {
            smoothedTime += 0.0005f * (targetTime - smoothedTime);
            const float readPos = static_cast<float>(writeIndex) - smoothedTime;
            const int i0 = static_cast<int>(std::floor(readPos));
            const float frac = readPos - static_cast<float>(i0);

            auto read = [&](int channel) noexcept
            {
                const auto& line = lines[static_cast<std::size_t>(channel)];
                const float a = line[static_cast<std::size_t>(i0 & mask)];
                const float b = line[static_cast<std::size_t>((i0 + 1) & mask)];
                return a + (b - a) * frac;
            };

            const float tapL = read(0);
            const float tapR = read(1);

            lpState[0] += lpCoef * (tapL - lpState[0]);
            lpState[1] += lpCoef * (tapR - lpState[1]);

            // Light ping-pong: feedback crosses channels a little. The tape
            // saturation is referenced to the bus's nominal level so it keeps
            // the same character now that the bus runs with headroom (#2).
            lines[0][static_cast<std::size_t>(writeIndex)] =
                left[i] + saturate((lpState[0] * 0.8f + lpState[1] * 0.2f) * feedback);
            lines[1][static_cast<std::size_t>(writeIndex)] =
                right[i] + saturate((lpState[1] * 0.8f + lpState[0] * 0.2f) * feedback);

            // Crossfade, not a boost (issue #2, fault 2).
            left[i] += mix * (tapL - left[i]);
            right[i] += mix * (tapR - right[i]);
            writeIndex = (writeIndex + 1) & mask;
        }
    }

private:
    static int nextPow2(int v) noexcept { int p = 1; while (p < v) p <<= 1; return p; }

    float saturate(float x) const noexcept
    {
        return fastTanh(x / levelReference) * levelReference;
    }

    std::vector<float> lines[2];
    float levelReference { 1.0f };
    float lpState[2] { 0.0f, 0.0f };
    double sr { 48000.0 };
    int writeIndex { 0 };
    int mask { 0 };
    float targetTime { 16000.0f };
    float smoothedTime { 16000.0f };
    float feedback { 0.3f };
    float mix { 0.0f };
};

} // namespace sappsynth
