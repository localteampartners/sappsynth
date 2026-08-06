#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "dsp/effects/Chorus.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Reverb.h"
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

TEST_CASE("Delay produces an echo at the configured time")
{
    const double sr = 48000.0;
    StereoDelay delay;
    delay.prepare(sr);
    delay.setParameters(0.25f, 0.0f, 1.0f);

    const int n = 24000;
    std::vector<float> left(static_cast<std::size_t>(n), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(n), 0.0f);
    left[0] = right[0] = 1.0f;
    delay.process(left.data(), right.data(), n);

    // Find the echo peak (skip the dry impulse).
    int peakAt = 0;
    float peakVal = 0.0f;
    for (int i = 100; i < n; ++i)
        if (std::abs(left[static_cast<std::size_t>(i)]) > peakVal)
        {
            peakVal = std::abs(left[static_cast<std::size_t>(i)]);
            peakAt = i;
        }
    // Time smoothing means the first echo lands near (not exactly at) 0.25 s.
    REQUIRE(peakVal > 0.2f);
    REQUIRE(std::abs(peakAt - 12000) < 2400);
}

TEST_CASE("Reverb generates a decaying tail")
{
    const double sr = 48000.0;
    Reverb reverb;
    reverb.prepare(sr);
    reverb.setParameters(0.7f, 1.0f);

    const int n = 96000;
    std::vector<float> left(static_cast<std::size_t>(n), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(n), 0.0f);
    left[0] = right[0] = 1.0f;
    reverb.process(left.data(), right.data(), n);

    const double early = analyzer::rms(left.data() + 4800, 9600);   // 0.1-0.3 s
    const double late = analyzer::rms(left.data() + 72000, 9600);   // 1.5-1.7 s
    REQUIRE(early > 1e-4);       // tail exists
    REQUIRE(late < early);       // and decays
    for (const float v : left)
        REQUIRE(std::isfinite(v));
}

TEST_CASE("Chorus thickens without instability")
{
    const double sr = 48000.0;
    Chorus chorus;
    chorus.prepare(sr);
    chorus.setParameters(0.8f, 1.0f);

    const int n = 48000;
    std::vector<float> left(static_cast<std::size_t>(n));
    std::vector<float> right(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        left[static_cast<std::size_t>(i)] = right[static_cast<std::size_t>(i)] =
            std::sin(static_cast<float>(2.0 * 3.14159265358979 * 220.0 * i / sr)) * 0.5f;
    chorus.process(left.data(), right.data(), n);

    for (const float v : left)
    {
        REQUIRE(std::isfinite(v));
        REQUIRE(std::abs(v) < 2.0f);
    }
    // Stereo decorrelation: channels should differ audibly.
    double diff = 0.0;
    for (int i = 24000; i < n; ++i)
        diff += std::abs(static_cast<double>(left[static_cast<std::size_t>(i)])
                       - static_cast<double>(right[static_cast<std::size_t>(i)]));
    REQUIRE(diff / 24000.0 > 0.01);
}

TEST_CASE("Unison spreads energy around the fundamental and stays gain-compensated")
{
    auto render = [&](int unisonCount)
    {
        PatchState patch;
        patch.osc1.level = 1.0f;
        patch.cutoffHz = 8000.0f;
        patch.unisonCount = unisonCount;
        patch.unisonDetuneCents = 25.0f;
        patch.characterAmount = 0.0f;
        patch.driftAmountCents = 0.0f;
        SynthEngine engine;
        engine.setUnitSeed(31);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        return OfflineRenderer::renderSingleNote(engine, 60, 0.8f, 0.9, 1.0);
    };

    const auto single = render(1);
    const auto stacked = render(5);

    const double rmsSingle = analyzer::rms(single.left.data() + 12000, 24000);
    const double rmsStacked = analyzer::rms(stacked.left.data() + 12000, 24000);
    INFO("rms single " << rmsSingle << " stacked " << rmsStacked);
    // Gain compensation keeps loudness in the same ballpark (within ~6 dB).
    REQUIRE(rmsStacked > rmsSingle * 0.5);
    REQUIRE(rmsStacked < rmsSingle * 2.5);

    // Detuned members beat: the stacked render's amplitude envelope moves more.
    auto envelopeWobble = [](const std::vector<float>& x)
    {
        double minRms = 1e9, maxRms = 0.0;
        for (int w = 0; w < 8; ++w)
        {
            const double r = analyzer::rms(x.data() + 12000 + w * 3000, 3000);
            minRms = std::min(minRms, r);
            maxRms = std::max(maxRms, r);
        }
        return maxRms / std::max(minRms, 1e-9);
    };
    REQUIRE(envelopeWobble(stacked.left) > envelopeWobble(single.left));
}

TEST_CASE("Glide starts at the previous pitch and lands on the target")
{
    PatchState patch;
    patch.osc1.waveform = Waveform::Sine;
    patch.cutoffHz = 16000.0f;
    patch.glideSeconds = 0.3f;
    patch.characterAmount = 0.0f;
    patch.driftAmountCents = 0.0f;
    patch.ampAttack = 0.001f;

    SynthEngine engine;
    engine.setUnitSeed(8);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    std::vector<TimedEvent> events {
        { 0.0, { Event::Type::NoteOn, 0, 48, 0.9f } },
        { 0.5, { Event::Type::NoteOff, 0, 48, 0.0f } },
        { 0.5, { Event::Type::NoteOn, 0, 72, 0.9f } }, // two octaves up
    };
    const auto result = OfflineRenderer::render(engine, events, 2.5);

    const int sr = 48000;
    auto freqAt = [&](double seconds)
    {
        const int n = 8192;
        const auto spectrum = analyzer::magnitudeSpectrum(
            result.left.data() + static_cast<int>(seconds * sr), n, n);
        return analyzer::peakFrequencyHz(spectrum, sr);
    };

    const double early = freqAt(0.52);  // just after the second note-on
    const double late = freqAt(2.0);    // fully settled
    INFO("early " << early << " Hz, late " << late << " Hz");
    REQUIRE(late > noteToHz(72) * 0.97);
    REQUIRE(late < noteToHz(72) * 1.03);
    REQUIRE(early < late * 0.8);        // audibly below target while gliding
}
