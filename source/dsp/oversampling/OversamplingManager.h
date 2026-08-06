#pragma once
#include <array>
#include <cmath>
#include <cstring>

namespace sappsynth {

// 2x half-band FIR up/down stage. Kernel is generated at prepare time
// (windowed sinc, Blackman) so there are no magic coefficient tables.
class HalfbandStage
{
public:
    static constexpr int kTaps = 31;

    void prepare() noexcept
    {
        constexpr int center = kTaps / 2;
        double sum = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const int m = n - center;
            double v;
            if (m == 0)
                v = 0.5;
            else if ((m & 1) == 0)
                v = 0.0; // half-band: even taps are zero
            else
            {
                const double x = 3.14159265358979323846 * m / 2.0;
                const double sinc = std::sin(x) / x * 0.5;
                const double w = 0.42 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * n / (kTaps - 1))
                               + 0.08 * std::cos(4.0 * 3.14159265358979323846 * n / (kTaps - 1));
                v = sinc * w;
            }
            h[static_cast<std::size_t>(n)] = static_cast<float>(v);
            sum += v;
        }
        // Normalize DC gain to exactly 1.
        for (auto& c : h)
            c = static_cast<float>(c / sum);
        reset();
    }

    void reset() noexcept
    {
        upState.fill(0.0f);
        downState.fill(0.0f);
    }

    // n input samples -> 2n output samples.
    void upsample(const float* in, float* out, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            push(upState, in[i]);
            out[2 * i]     = 2.0f * convolveEven(upState);
            out[2 * i + 1] = 2.0f * convolveOdd(upState);
        }
    }

    // 2n input samples -> n output samples.
    void downsample(const float* in, float* out, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            push(downState, in[2 * i]);
            push(downState, in[2 * i + 1]);
            out[i] = convolveAll(downState);
        }
    }

private:
    using Delay = std::array<float, kTaps>;

    static void push(Delay& d, float x) noexcept
    {
        std::memmove(d.data() + 1, d.data(), (kTaps - 1) * sizeof(float));
        d[0] = x;
    }

    float convolveAll(const Delay& d) const noexcept
    {
        float acc = 0.0f;
        for (int n = 0; n < kTaps; ++n)
            acc += h[static_cast<std::size_t>(n)] * d[static_cast<std::size_t>(n)];
        return acc;
    }

    // Polyphase halves for zero-stuffed interpolation: even branch sees the
    // zero taps collapse to the center coefficient.
    float convolveEven(const Delay& d) const noexcept
    {
        float acc = 0.0f;
        for (int n = 0; n < kTaps; n += 2)
            acc += h[static_cast<std::size_t>(n)] * d[static_cast<std::size_t>(n / 2)];
        return acc;
    }

    float convolveOdd(const Delay& d) const noexcept
    {
        float acc = 0.0f;
        for (int n = 1; n < kTaps; n += 2)
            acc += h[static_cast<std::size_t>(n)] * d[static_cast<std::size_t>(n / 2)];
        return acc;
    }

    std::array<float, kTaps> h {};
    Delay upState {};
    Delay downState {};
};

// Oversampled island (architecture §12): the nonlinear mixer -> ladder -> VCA
// chain runs at 1x/2x/4x depending on quality mode. Fixed-capacity scratch,
// no allocation after prepare.
class OversamplingManager
{
public:
    static constexpr int kMaxBaseBlock = 64;

    void prepare() noexcept
    {
        stageA.prepare();
        stageB.prepare();
        setFactor(1);
    }

    void reset() noexcept
    {
        stageA.reset();
        stageB.reset();
    }

    void setFactor(int newFactor) noexcept
    {
        factor = (newFactor == 2 || newFactor == 4) ? newFactor : 1;
    }

    int currentFactor() const noexcept { return factor; }

    // processor(sample, baseIndex) is called once per oversampled sample with
    // the base-rate index it belongs to (for envelope lookup).
    template <typename Fn>
    void process(float* buffer, int n, Fn&& processor) noexcept
    {
        if (factor == 1)
        {
            for (int i = 0; i < n; ++i)
                buffer[i] = processor(buffer[i], i);
            return;
        }

        if (factor == 2)
        {
            stageA.upsample(buffer, scratch2.data(), n);
            for (int i = 0; i < 2 * n; ++i)
                scratch2[static_cast<std::size_t>(i)] = processor(scratch2[static_cast<std::size_t>(i)], i >> 1);
            stageA.downsample(scratch2.data(), buffer, n);
            return;
        }

        // 4x: two cascaded half-band stages.
        stageA.upsample(buffer, scratch2.data(), n);
        stageB.upsample(scratch2.data(), scratch4.data(), 2 * n);
        for (int i = 0; i < 4 * n; ++i)
            scratch4[static_cast<std::size_t>(i)] = processor(scratch4[static_cast<std::size_t>(i)], i >> 2);
        stageB.downsample(scratch4.data(), scratch2.data(), 2 * n);
        stageA.downsample(scratch2.data(), buffer, n);
    }

private:
    HalfbandStage stageA, stageB;
    std::array<float, 2 * kMaxBaseBlock> scratch2 {};
    std::array<float, 4 * kMaxBaseBlock> scratch4 {};
    int factor { 1 };
};

} // namespace sappsynth
