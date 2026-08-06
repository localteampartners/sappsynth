#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/variation/DriftProcess.h"
#include "dsp/variation/ComponentProfile.h"
#include "dsp/variation/ThermalModel.h"

using namespace sappsynth;

TEST_CASE("OU drift stays bounded over minutes and actually moves")
{
    const double sr = 48000.0;
    DriftProcess drift;
    drift.prepare(sr);
    drift.seed(777);
    const float theta = 1.0f;
    drift.setParameters(theta, std::sqrt(2.0f * theta)); // stationary std = 1

    float maxAbs = 0.0f;
    double sumSq = 0.0;
    const int chunks = static_cast<int>(120.0 * sr) / 32; // 2 minutes
    for (int i = 0; i < chunks; ++i)
    {
        const float v = drift.process(32);
        maxAbs = std::max(maxAbs, std::abs(v));
        sumSq += static_cast<double>(v) * v;
    }
    const double measuredStd = std::sqrt(sumSq / chunks);
    INFO("max " << maxAbs << " std " << measuredStd);
    REQUIRE(maxAbs < 6.0f);        // bounded (< 6 sigma)
    REQUIRE(measuredStd > 0.3);    // actually wanders
    REQUIRE(measuredStd < 2.0);
}

TEST_CASE("Drift is smooth: no white-noise fuzz between control ticks")
{
    const double sr = 48000.0;
    DriftProcess drift;
    drift.prepare(sr, 50.0f);
    drift.seed(123);
    drift.setParameters(1.0f, std::sqrt(2.0f));

    float previous = drift.process(8);
    for (int i = 0; i < 50000; ++i)
    {
        const float v = drift.process(8);
        // 8 samples apart the interpolated process must move very little.
        REQUIRE(std::abs(v - previous) < 0.05f);
        previous = v;
    }
}

TEST_CASE("Drift is deterministic per seed")
{
    DriftProcess a, b;
    a.prepare(48000.0);
    b.prepare(48000.0);
    a.seed(9); b.seed(9);
    a.setParameters(1.0f, 1.4f);
    b.setParameters(1.0f, 1.4f);
    for (int i = 0; i < 1000; ++i)
        REQUIRE(a.process(32) == b.process(32));
}

TEST_CASE("Component profiles are stable per seed and differ across voices")
{
    const auto p0 = VoiceProfile::generate(seeds::voiceSeed(42, 0));
    const auto p0Again = VoiceProfile::generate(seeds::voiceSeed(42, 0));
    const auto p1 = VoiceProfile::generate(seeds::voiceSeed(42, 1));

    REQUIRE(p0.osc1TuneCents == p0Again.osc1TuneCents);
    REQUIRE(p0.filterCutoffCents == p0Again.filterCutoffCents);
    REQUIRE(p0.osc1TuneCents != p1.osc1TuneCents);

    // Tolerances are subtle by design: tuning within a handful of cents.
    REQUIRE(std::abs(p0.osc1TuneCents) < 12.0f);
    REQUIRE(std::abs(p0.envTimeFactor - 1.0f) < 0.2f);
}

TEST_CASE("Thermal warm-up decays monotonically toward zero")
{
    ThermalModel thermal;
    thermal.prepare(48000.0);
    thermal.configure(10.0f, 30.0f, 0.0f); // fully cold start
    thermal.coldStart();

    float previous = thermal.currentCents();
    REQUIRE(previous > 9.0f);
    for (int i = 0; i < 100; ++i)
    {
        const float v = thermal.advance(48000); // 1s steps
        REQUIRE(v <= previous + 1e-6f);
        previous = v;
    }
    REQUIRE(previous < 0.5f); // ~100s >> tau 30s
}
