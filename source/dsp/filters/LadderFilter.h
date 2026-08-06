#pragma once
#include <algorithm>
#include <cmath>
#include "../utility/FastMath.h"

namespace sappsynth {

// Zero-delay-feedback four-pole ladder (architecture §11). Topology-preserving
// one-pole stages with tanh saturation *inside* each stage, nonlinear feedback,
// and a fixed-point solver whose iteration count is the quality knob
// (Eco=1 ≈ one-sample feedback, Normal=2, High=3). Input level audibly changes
// resonance character — that is the point of putting the nonlinearities here.
class LadderFilter
{
public:
    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (auto& stage : s)
            stage = 0.0f;
        previousY4 = 0.0f;
    }

    void setSolverIterations(int iterations) noexcept
    {
        solverIterations = std::clamp(iterations, 1, 4);
    }

    // The expensive tan() lives here so callers can compute G sparsely (chunk
    // edges) and interpolate — per-sample setG is just a store.
    float computeG(float cutoffHz) const noexcept
    {
        const double fc = std::clamp(static_cast<double>(cutoffHz), 10.0, sr * 0.45);
        const double g = std::tan(kPi * fc / sr);
        return static_cast<float>(g / (1.0 + g));
    }

    void setG(float newG) noexcept { G = newG; }

    // resonance 0..1 (self-oscillation starts ~0.9); drive linear (1 = unity).
    void setTone(float resonance, float driveLinear) noexcept
    {
        // Panel resonance -> feedback gain. 4.0 is the linear self-osc limit;
        // the tanh stages let us push slightly past it for a confident sine.
        k = 4.3f * std::clamp(resonance, 0.0f, 1.1f);
        drive = std::max(driveLinear, 0.01f);
        // Passband loss compensation: resonance steals low end; give some back
        // before the input tanh so it also drives the ladder harder.
        inputMakeup = 1.0f + 0.85f * std::min(k, 4.0f) * 0.25f;
        outputMakeup = 1.0f / std::pow(std::max(drive, 1.0f), 0.6f);
    }

    void setParameters(float cutoffHz, float resonance, float driveLinear) noexcept
    {
        setTone(resonance, driveLinear);
        setG(computeG(cutoffHz));
    }

    float process(float x) noexcept
    {
        const float in = x * drive * inputMakeup;

        // Fixed-point iterations on the global feedback loop.
        float y4 = previousY4;
        float y1 = 0.0f, y2 = 0.0f, y3 = 0.0f;
        for (int i = 0; i < solverIterations; ++i)
        {
            const float u = fastTanh(in - k * fastTanh(y4));
            y1 = preview(s[0], u);
            y2 = preview(s[1], fastTanh(y1));
            y3 = preview(s[2], fastTanh(y2));
            y4 = preview(s[3], fastTanh(y3));
        }

        // Commit states with the converged loop value.
        const float u = fastTanh(in - k * fastTanh(y4));
        y1 = commit(s[0], u);
        y2 = commit(s[1], fastTanh(y1));
        y3 = commit(s[2], fastTanh(y2));
        y4 = commit(s[3], fastTanh(y3));

        previousY4 = flushDenormal(y4);
        return y4 * outputMakeup;
    }

private:
    float preview(float state, float x) const noexcept
    {
        const float v = (x - state) * G;
        return v + state;
    }

    float commit(float& state, float x) const noexcept
    {
        const float v = (x - state) * G;
        const float y = v + state;
        state = flushDenormal(y + v);
        return y;
    }

    double sr { 48000.0 };
    float s[4] { 0, 0, 0, 0 };
    float previousY4 { 0.0f };
    float G { 0.1f };
    float k { 0.0f };
    float drive { 1.0f };
    float inputMakeup { 1.0f };
    float outputMakeup { 1.0f };
    int solverIterations { 2 };
};

} // namespace sappsynth
