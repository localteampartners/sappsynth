#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/filters/LadderFilter.h"
#include "dsp/variation/RandomSource.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

namespace {

// Measure gain at one frequency by running a sine through the filter.
double gainAt(LadderFilter& filter, double freq, double sr)
{
    const int n = 24000;
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const float x = std::sin(static_cast<float>(2.0 * 3.14159265358979 * freq * i / sr)) * 0.2f;
        out[static_cast<std::size_t>(i)] = filter.process(x);
    }
    // Compare steady-state RMS against input RMS (skip transient).
    const double outRms = analyzer::rms(out.data() + n / 2, n / 2);
    return outRms / (0.2 * 0.70710678);
}

} // namespace

TEST_CASE("Ladder low-pass attenuates far above cutoff and passes far below")
{
    const double sr = 96000.0; // pretend 2x island rate
    LadderFilter filter;
    filter.prepare(sr);
    filter.setSolverIterations(2);
    filter.setParameters(1000.0f, 0.0f, 1.0f);

    filter.reset();
    const double lowGain = gainAt(filter, 100.0, sr);
    filter.reset();
    const double highGain = gainAt(filter, 8000.0, sr);

    INFO("low " << lowGain << " high " << highGain);
    REQUIRE(lowGain > 0.7);   // passband roughly unity
    REQUIRE(highGain < 0.01); // ~ -40 dB or better 3 octaves up (24 dB/oct)
}

TEST_CASE("Filter is stable across sample rates, cutoffs and resonance")
{
    RandomSource rng(4242);
    for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (const float cutoff : { 30.0f, 500.0f, 5000.0f, 18000.0f, 30000.0f })
        {
            LadderFilter filter;
            filter.prepare(sr);
            filter.setSolverIterations(2);
            filter.setParameters(cutoff, 1.05f, 4.0f); // hostile settings
            float maxOut = 0.0f;
            for (int i = 0; i < 48000; ++i)
            {
                const float y = filter.process(rng.nextSigned() * 0.8f);
                REQUIRE(std::isfinite(y));
                maxOut = std::max(maxOut, std::abs(y));
            }
            INFO("sr " << sr << " cutoff " << cutoff << " max " << maxOut);
            REQUIRE(maxOut < 10.0f);
        }
    }
}

TEST_CASE("Self-oscillation produces a tone near the cutoff frequency")
{
    const double sr = 96000.0;
    LadderFilter filter;
    filter.prepare(sr);
    filter.setSolverIterations(3);
    filter.setParameters(880.0f, 1.05f, 1.0f);

    // Kick it with a tiny impulse, then run silent input.
    filter.process(0.1f);
    const int n = 32768;
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < 48000; ++i)
        filter.process(0.0f); // settle into oscillation
    for (int i = 0; i < n; ++i)
        out[static_cast<std::size_t>(i)] = filter.process(0.0f);

    const double level = analyzer::rms(out.data(), n);
    REQUIRE(level > 0.05); // it actually oscillates

    const auto spectrum = analyzer::magnitudeSpectrum(out.data(), n, n);
    const double freq = analyzer::peakFrequencyHz(spectrum, sr);
    INFO("self-oscillation at " << freq << " Hz for cutoff 880");
    // Within a musical third of the cutoff — tuning compensation is TODO-level
    // precision, stability of pitch is what matters here.
    REQUIRE(freq > 700.0);
    REQUIRE(freq < 1100.0);
}

TEST_CASE("Input drive changes harmonic content, not just level")
{
    const double sr = 96000.0;
    auto thdish = [&](float drive)
    {
        LadderFilter filter;
        filter.prepare(sr);
        filter.setSolverIterations(2);
        filter.setParameters(5000.0f, 0.3f, drive);
        const int n = 16384;
        std::vector<float> out(static_cast<std::size_t>(n));
        for (int i = 0; i < 8000; ++i)
            filter.process(std::sin(static_cast<float>(2.0 * 3.14159265358979 * 220.0 * i / sr)) * 0.5f);
        for (int i = 0; i < n; ++i)
            out[static_cast<std::size_t>(i)] =
                filter.process(std::sin(static_cast<float>(2.0 * 3.14159265358979 * 220.0 * (i + 8000) / sr)) * 0.5f);
        const auto spectrum = analyzer::magnitudeSpectrum(out.data(), n, n);
        // Energy at 2nd+3rd harmonic relative to fundamental.
        const double binWidth = sr / n;
        const auto bin = [&](double f) { return spectrum[static_cast<std::size_t>(f / binWidth + 0.5)]; };
        return (bin(440.0) + bin(660.0)) / std::max(bin(220.0), 1e-12);
    };

    const double clean = thdish(0.5f);
    const double driven = thdish(8.0f);
    INFO("harmonic ratio clean " << clean << " driven " << driven);
    REQUIRE(driven > clean * 3.0);
}
