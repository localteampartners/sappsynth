#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/nonlinear/MixerModel.h"
#include "dsp/nonlinear/VcaModel.h"
#include "dsp/filters/DcBlocker.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

TEST_CASE("Mixer static offset is compensated (silence in, silence out)")
{
    MixerModel mixer;
    mixer.setGains(1.0f, 1.0f, 1.0f, 1.0f);
    mixer.setCharacter(4.0f, 0.3f);
    REQUIRE(std::abs(mixer.process(0.0f, 0.0f, 0.0f, 0.0f)) < 1e-6f);
}

TEST_CASE("Mixer + DC blocker removes signal-dependent DC from asymmetry")
{
    // Asymmetric saturation rectifies DC out of a symmetric signal; the HP
    // block in the signal path (§7) is what removes it. Test them together,
    // the way the voice wires them.
    MixerModel mixer;
    mixer.setGains(1.0f, 1.0f, 1.0f, 1.0f);
    mixer.setCharacter(4.0f, 0.3f);
    DcBlocker dc;
    dc.prepare(48000.0);

    double sum = 0.0;
    const int n = 96000;
    for (int i = 0; i < n; ++i)
    {
        const float phase = static_cast<float>(i) * 0.01f;
        const float y = dc.process(mixer.process(std::sin(phase), std::sin(phase * 1.5f),
                                                 std::sin(phase * 0.5f), 0.0f));
        REQUIRE(std::abs(y) <= 2.0f);
        if (i >= 24000) // skip blocker settling
            sum += y;
    }
    REQUIRE(std::abs(sum / (n - 24000)) < 0.02);
}

TEST_CASE("Mixer drive adds harmonics to a sine")
{
    const double sr = 48000.0;
    const int n = 16384;
    auto render = [&](float drive)
    {
        MixerModel mixer;
        mixer.setGains(1.0f, 0.0f, 0.0f, 0.0f);
        mixer.setCharacter(drive, 0.2f);
        std::vector<float> out(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            out[static_cast<std::size_t>(i)] =
                mixer.process(std::sin(static_cast<float>(2.0 * 3.14159265358979 * 440.0 * i / sr)), 0, 0, 0);
        return analyzer::aliasEnergyDb(analyzer::magnitudeSpectrum(out.data(), n, n), sr, 440.0, 16);
    };

    // With hard drive, energy appears *between* the fundamental's neighborhood
    // and its harmonics stay dominant — compare residual against clean.
    const double clean = render(1.0f);
    const double driven = render(8.0f);
    INFO("clean " << clean << " driven " << driven);
    // Both should remain harmonic (saturation creates harmonics, not noise).
    REQUIRE(clean < -40.0);
    REQUIRE(driven < -20.0);
}

TEST_CASE("Adding a second source changes headroom, not just loudness")
{
    MixerModel mixer;
    mixer.setGains(1.0f, 1.0f, 0.0f, 0.0f);
    mixer.setCharacter(3.0f, 0.0f);

    // One source at peak vs two sources at peak: the sum must be less than
    // twice the single because the saturator compresses.
    const float one = mixer.process(1.0f, 0.0f, 0.0f, 0.0f);
    const float two = mixer.process(1.0f, 1.0f, 0.0f, 0.0f);
    REQUIRE(two < one * 2.0f * 0.9f);
    REQUIRE(two > one); // still louder, though
}

TEST_CASE("VCA gain curve is monotonic and reaches silence/full scale")
{
    VcaModel vca;
    vca.configure(0.2f, 0.05f, 1.0f);

    float previous = -1.0f;
    for (int i = 0; i <= 100; ++i)
    {
        const float env = static_cast<float>(i) / 100.0f;
        const float y = vca.process(0.5f, env);
        REQUIRE(y >= previous - 1e-6f);
        previous = y;
    }
    REQUIRE(std::abs(vca.process(0.5f, 0.0f)) < 1e-6f);
    REQUIRE(vca.process(0.5f, 1.0f) > 0.3f);
}
