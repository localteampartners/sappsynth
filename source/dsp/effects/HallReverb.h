#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "../utility/FastMath.h"

namespace sappsynth {

// THE HALL (v0.12). An 8-line feedback delay network with Householder
// feedback, input diffusion, per-line modulation, in-loop damping, a
// predelay and a wet hi-cut — ported from sapporchestra's stage hall and
// given the controls a synth pad needs (predelay, damping, modulation,
// hi-cut, 0.6-12 s tails). The Freeverb it replaces had fixed damping, no
// predelay and no modulation, which is why nothing here could do an M83 or
// Chicane pad: a static comb bank rings metallic on a sustained saw stack,
// an FDN with slowly moving taps does not.
//
// Gain staging follows issue #2: the wet path is normalised so full wet
// sits at the dry signal's broadband level and `Size` buys tail, not
// loudness; `mix` is a linear crossfade, never a boost.
class HallReverb
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        static constexpr float baseMs[kLines] { 29.7f, 37.1f, 41.1f, 47.3f, 53.9f, 61.7f, 71.9f, 83.3f };
        for (int i = 0; i < kLines; ++i)
        {
            baseSamples[i] = static_cast<float>(baseMs[i] * 0.001 * sampleRate);
            lines[static_cast<std::size_t>(i)].assign(static_cast<std::size_t>(baseSamples[i] * 2.4f) + 64, 0.0f);
            writePos[i] = 0;
            dampState[i] = 0.0f;
            lfoPhase[i] = static_cast<float>(i) * 0.785f;
        }
        inDiffL.prepare(sampleRate, 4.7f, 0.62f);
        inDiffR.prepare(sampleRate, 3.6f, 0.62f);
        predelayLine.assign(static_cast<std::size_t>(sampleRate * 0.25) + 8, 0.0f);
        predelayWrite = 0;
        hiCutState[0] = hiCutState[1] = 0.0f;
        update();
    }

    // size 0..1 (room size AND tail: 0.6 s at 0 to ~12 s at 1), mix 0..1,
    // predelay ms, damping 0..1, modulation depth 0..1, hi-cut Hz.
    void setParameters(float size01, float mixAmount, float predelayMs = 12.0f, float damping01 = 0.45f,
                       float modDepth01 = 0.35f, float hiCutHz = 9000.0f) noexcept
    {
        const float s = std::clamp(size01, 0.0f, 1.0f);
        size = s;
        sizeScale = 0.55f + 0.95f * s;
        decaySeconds = 0.6f + 11.4f * s * s;
        damping = std::clamp(damping01, 0.0f, 1.0f);
        modDepth = std::clamp(modDepth01, 0.0f, 1.0f);
        predelaySamples = std::clamp(static_cast<float>(predelayMs * 0.001 * sr), 0.0f,
                                     static_cast<float>(predelayLine.size()) - 4.0f);
        hiCutCoef = 1.0f - std::exp(-static_cast<float>(kTwoPi) * std::clamp(hiCutHz, 500.0f, 20000.0f)
                                    / static_cast<float>(sr));
        mix = std::clamp(mixAmount, 0.0f, 1.0f);
        update();
    }

    void process(float* left, float* right, int n) noexcept
    {
        if (mix <= 0.0001f)
            return;
        const float k = 2.0f / static_cast<float>(kLines);
        for (int i = 0; i < n; ++i)
        {
            // predelay on the mono send
            const float send = 0.5f * (left[i] + right[i]);
            predelayLine[static_cast<std::size_t>(predelayWrite)] = send;
            float rp = static_cast<float>(predelayWrite) - predelaySamples;
            const int psz = static_cast<int>(predelayLine.size());
            while (rp < 0.0f) rp += static_cast<float>(psz);
            const int p0 = static_cast<int>(rp);
            const float pf = rp - static_cast<float>(p0);
            const int p1 = p0 + 1 >= psz ? 0 : p0 + 1;
            const float delayed = predelayLine[static_cast<std::size_t>(p0)] * (1.0f - pf)
                                + predelayLine[static_cast<std::size_t>(p1)] * pf;
            if (++predelayWrite >= psz) predelayWrite = 0;
            const float inMono = 0.5f * (inDiffL.tick(delayed) + inDiffR.tick(delayed)) * wetFeed;

            float read[kLines];
            float sum = 0.0f;
            for (int l = 0; l < kLines; ++l)
            {
                lfoPhase[l] += lfoInc[l];
                if (lfoPhase[l] > static_cast<float>(kTwoPi)) lfoPhase[l] -= static_cast<float>(kTwoPi);
                const float delay = delaySamples[l] + std::sin(lfoPhase[l]) * modSamples;
                auto& line = lines[static_cast<std::size_t>(l)];
                const int size = static_cast<int>(line.size());
                float pos = static_cast<float>(writePos[l]) - delay;
                while (pos < 0.0f) pos += static_cast<float>(size);
                const int i0 = static_cast<int>(pos);
                const float frac = pos - static_cast<float>(i0);
                const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
                read[l] = line[static_cast<std::size_t>(i0)] * (1.0f - frac) + line[static_cast<std::size_t>(i1)] * frac;
                sum += read[l];
            }
            for (int l = 0; l < kLines; ++l)
            {
                float v = feedback[l] * (read[l] - k * sum) + inMono;
                dampState[l] = flushDenormal(dampState[l] + dampCoef * (v - dampState[l]));
                v = dampState[l];
                auto& line = lines[static_cast<std::size_t>(l)];
                line[static_cast<std::size_t>(writePos[l])] = v;
                if (++writePos[l] >= static_cast<int>(line.size())) writePos[l] = 0;
            }
            float wetL = (read[0] - read[2] + read[4] - read[6]) * 0.5f;
            float wetR = (read[1] - read[3] + read[5] - read[7]) * 0.5f;
            // wet hi-cut: air rolls off the way a real room's does
            hiCutState[0] = flushDenormal(hiCutState[0] + hiCutCoef * (wetL - hiCutState[0]));
            hiCutState[1] = flushDenormal(hiCutState[1] + hiCutCoef * (wetR - hiCutState[1]));
            wetL = hiCutState[0];
            wetR = hiCutState[1];
            // crossfade, not a boost (issue #2)
            left[i] += mix * (wetL - left[i]);
            right[i] += mix * (wetR - right[i]);
        }
    }

    float decayTimeSeconds() const noexcept { return decaySeconds; }

private:
    static constexpr int kLines = 8;

    struct Allpass
    {
        void prepare(double sampleRate, float ms, float g) noexcept
        {
            buffer.assign(static_cast<std::size_t>(ms * 0.001 * sampleRate) + 2, 0.0f);
            pos = 0;
            gain = g;
        }
        float tick(float x) noexcept
        {
            const float d = buffer[static_cast<std::size_t>(pos)];
            const float y = -gain * x + d;
            buffer[static_cast<std::size_t>(pos)] = flushDenormal(x + gain * y);
            if (++pos >= static_cast<int>(buffer.size())) pos = 0;
            return y;
        }
        std::vector<float> buffer;
        int pos { 0 };
        float gain { 0.6f };
    };

    void update() noexcept
    {
        float meanLoss = 0.0f;
        for (int l = 0; l < kLines; ++l)
        {
            delaySamples[l] = std::min(baseSamples[l] * sizeScale,
                                       static_cast<float>(lines[static_cast<std::size_t>(l)].size()) - 12.0f);
            // per-line gain for a uniform T60 across all line lengths
            feedback[l] = std::pow(10.0f, -3.0f * delaySamples[l] / (decaySeconds * static_cast<float>(sr)));
            lfoInc[l] = static_cast<float>((0.31 + 0.13 * l) * kTwoPi / sr);
            meanLoss += 1.0f - feedback[l] * feedback[l];
        }
        meanLoss /= static_cast<float>(kLines);
        dampCoef = 1.0f - 0.85f * damping;
        modSamples = 1.0f + modDepth * 6.0f;
        // Normalise the wet level: the steady-state power of a lossy loop fed
        // with white noise goes as 1/(1-g^2), so feeding sqrt(1-g^2) holds it
        // across the tail range; the damping and the longer lines of a big
        // room still take ~3.5 dB off the top, hence the size tilt. Both
        // constants are measured (tests/unit/test_effects.cpp, "Hall: full
        // wet"): full wet lands at the dry broadband level within a dB.
        wetFeed = kWetReference * (1.0f + kSizeTilt * size) * std::sqrt(std::max(meanLoss, 1.0e-4f));
    }

    static constexpr float kWetReference = 3.75f;
    static constexpr float kSizeTilt = 0.51f;

    double sr { 48000.0 };
    std::array<std::vector<float>, kLines> lines {};
    float baseSamples[kLines] {};
    float delaySamples[kLines] {};
    float feedback[kLines] {};
    float dampState[kLines] {};
    float lfoPhase[kLines] {};
    float lfoInc[kLines] {};
    int writePos[kLines] {};
    Allpass inDiffL, inDiffR;
    std::vector<float> predelayLine;
    int predelayWrite { 0 };
    float predelaySamples { 0.0f };
    float hiCutCoef { 0.5f };
    float hiCutState[2] { 0.0f, 0.0f };

    float size { 0.5f }, sizeScale { 1.0f }, decaySeconds { 2.6f }, damping { 0.45f }, modDepth { 0.35f };
    float dampCoef { 0.66f }, modSamples { 3.0f };
    float mix { 0.0f };
    float wetFeed { 1.0f };
};

} // namespace sappsynth
