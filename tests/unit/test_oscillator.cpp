#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/oscillators/PhaseAccumulator.h"
#include "dsp/oscillators/BandLimitedOscillator.h"
#include "dsp/utility/FastMath.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

namespace {

std::vector<float> renderOsc(Waveform wave, OscillatorMethod method,
                             double freq, double sr, int n)
{
    BandLimitedOscillator osc;
    osc.reset();
    osc.setWaveform(wave);
    osc.setMethod(method);
    std::vector<float> out(static_cast<std::size_t>(n));
    const double inc = freq / sr;
    for (int i = 0; i < n; ++i)
        out[static_cast<std::size_t>(i)] = osc.tick(inc);
    return out;
}

} // namespace

TEST_CASE("Phase accumulator wraps exactly and never leaves [0,1)")
{
    PhaseAccumulator phase;
    phase.reset(0.999);
    int wraps = 0;
    for (int i = 0; i < 10000; ++i)
    {
        if (phase.advance(0.01))
            ++wraps;
        REQUIRE(phase.value() >= 0.0);
        REQUIRE(phase.value() < 1.0);
    }
    REQUIRE(wraps == 100); // 10000 * 0.01 cycles
}

TEST_CASE("noteToHz matches equal temperament")
{
    REQUIRE(std::abs(noteToHz(69.0) - 440.0) < 1e-9);
    REQUIRE(std::abs(noteToHz(57.0) - 220.0) < 1e-9);
    REQUIRE(std::abs(noteToHz(60.0) - 261.6255653) < 1e-4);
}

TEST_CASE("Saw oscillator produces the requested fundamental")
{
    const double sr = 48000.0;
    const double freq = noteToHz(57.0); // 220 Hz
    const auto signal = renderOsc(Waveform::Saw, OscillatorMethod::PolyBlep, freq, sr, 16384);
    const auto spectrum = analyzer::magnitudeSpectrum(signal.data(), 16384, 16384);
    const double detected = analyzer::peakFrequencyHz(spectrum, sr);
    REQUIRE(std::abs(detected - freq) < 2.0);
}

TEST_CASE("PolyBLEP saw aliases far less than naive saw at high pitch")
{
    // Note 96 (~2093 Hz): naive saw aliases badly, PolyBLEP should win by a
    // wide, regression-worthy margin (Lab experiment #1 as a test).
    const double sr = 48000.0;
    const double freq = noteToHz(96.0);
    const int n = 32768;

    const auto naive = renderOsc(Waveform::Saw, OscillatorMethod::Naive, freq, sr, n);
    const auto blep  = renderOsc(Waveform::Saw, OscillatorMethod::PolyBlep, freq, sr, n);

    const double naiveAlias = analyzer::aliasEnergyDb(
        analyzer::magnitudeSpectrum(naive.data(), n, n), sr, freq);
    const double blepAlias = analyzer::aliasEnergyDb(
        analyzer::magnitudeSpectrum(blep.data(), n, n), sr, freq);

    INFO("naive alias " << naiveAlias << " dB, polyblep alias " << blepAlias << " dB");
    REQUIRE(blepAlias < naiveAlias - 15.0);
    REQUIRE(blepAlias < -25.0);
}

TEST_CASE("Pulse wave has no DC for asymmetric pulse widths")
{
    const double sr = 48000.0;
    BandLimitedOscillator osc;
    osc.setWaveform(Waveform::Pulse);
    osc.setMethod(OscillatorMethod::PolyBlep);
    const int n = 48000;
    double sum = 0.0;
    const double inc = 220.0 / sr;
    for (int i = 0; i < n; ++i)
        sum += static_cast<double>(osc.tick(inc, 0.25f));
    REQUIRE(std::abs(sum / n) < 0.01);
}

TEST_CASE("Triangle amplitude is stable across frequencies")
{
    const double sr = 48000.0;
    for (const double freq : { 110.0, 440.0, 1760.0 })
    {
        const auto signal = renderOsc(Waveform::Triangle, OscillatorMethod::PolyBlep, freq, sr, 48000);
        // Skip the settling of the leaky integrator.
        const double p = analyzer::peak(signal.data() + 24000, 24000);
        INFO("freq " << freq << " peak " << p);
        REQUIRE(p > 0.7);
        REQUIRE(p < 1.3);
    }
}

TEST_CASE("Oscillators survive extreme increments without exploding")
{
    BandLimitedOscillator osc;
    osc.setWaveform(Waveform::Saw);
    for (int i = 0; i < 1000; ++i)
    {
        const float v = osc.tick(0.45); // near the clamp limit
        REQUIRE(std::isfinite(v));
        REQUIRE(std::abs(v) < 3.0f);
    }
}
