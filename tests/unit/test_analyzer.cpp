#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

TEST_CASE("FFT peak finder locates a pure sine within a fraction of a bin")
{
    const double sr = 48000.0;
    const double freq = 1234.5;
    const int n = 16384;
    std::vector<float> signal(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        signal[static_cast<std::size_t>(i)] =
            std::sin(static_cast<float>(2.0 * 3.14159265358979 * freq * i / sr));

    const auto spectrum = analyzer::magnitudeSpectrum(signal.data(), n, n);
    const double detected = analyzer::peakFrequencyHz(spectrum, sr);
    REQUIRE(std::abs(detected - freq) < 1.0);
}

TEST_CASE("Alias energy of a pure sine is far below its harmonic energy")
{
    const double sr = 48000.0;
    const int n = 16384;
    // Bin-aligned so Hann leakage stays inside the harmonic neighborhood.
    const double freq = sr / n * 341.0; // ~999 Hz
    std::vector<float> signal(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        signal[static_cast<std::size_t>(i)] =
            std::sin(static_cast<float>(2.0 * 3.14159265358979 * freq * i / sr));

    const auto spectrum = analyzer::magnitudeSpectrum(signal.data(), n, n);
    REQUIRE(analyzer::aliasEnergyDb(spectrum, sr, freq) < -60.0);
}

TEST_CASE("rms and peak are correct for a known signal")
{
    std::vector<float> signal(48000);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = std::sin(static_cast<float>(2.0 * 3.14159265358979 * 440.0 * static_cast<double>(i) / 48000.0));

    REQUIRE(std::abs(analyzer::rms(signal.data(), static_cast<int>(signal.size())) - 0.7071) < 0.01);
    REQUIRE(std::abs(analyzer::peak(signal.data(), static_cast<int>(signal.size())) - 1.0) < 0.01);
}
