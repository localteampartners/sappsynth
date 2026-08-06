#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

namespace {

PatchState arpPatch()
{
    PatchState patch;
    patch.osc1.level = 1.0f;
    patch.cutoffHz = 8000.0f;
    patch.ampAttack = 0.002f;
    patch.ampDecay = 0.05f;
    patch.ampSustain = 0.9f;
    patch.ampRelease = 0.03f;
    patch.characterAmount = 0.0f;
    patch.driftAmountCents = 0.0f;
    return patch;
}

// Count note onsets from the RMS envelope (rising edges through a threshold).
int countOnsets(const std::vector<float>& x, double sr)
{
    const int window = static_cast<int>(sr * 0.005);
    bool gateOpen = false;
    int onsets = 0;
    for (int start = 0; start + window < static_cast<int>(x.size()); start += window)
    {
        const double level = analyzer::rms(x.data() + start, window);
        if (!gateOpen && level > 0.02)
        {
            ++onsets;
            gateOpen = true;
        }
        else if (gateOpen && level < 0.005)
            gateOpen = false;
    }
    return onsets;
}

} // namespace

TEST_CASE("Arpeggiator steps at the configured rate with gaps between steps")
{
    PatchState patch = arpPatch();
    patch.arpMode = 1; // Up
    patch.arpRateHz = 8.0f;
    patch.arpGate = 0.4f;

    SynthEngine engine;
    engine.setUnitSeed(77);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    std::vector<TimedEvent> events {
        { 0.0, { Event::Type::NoteOn, 0, 48, 0.9f } },
        { 0.0, { Event::Type::NoteOn, 0, 52, 0.9f } },
        { 0.0, { Event::Type::NoteOn, 0, 55, 0.9f } },
    };
    const auto result = OfflineRenderer::render(engine, events, 2.0);

    const int onsets = countOnsets(result.left, 48000.0);
    INFO("onsets " << onsets << " (expect ~16 at 8 Hz over 2 s)");
    REQUIRE(onsets >= 12);
    REQUIRE(onsets <= 20);
}

TEST_CASE("Arpeggiator Up mode cycles through held notes in pitch order")
{
    PatchState patch = arpPatch();
    patch.arpMode = 1;
    patch.arpRateHz = 4.0f; // 250 ms per step: easy to isolate
    patch.arpGate = 0.9f;
    patch.osc1.waveform = Waveform::Sine;

    SynthEngine engine;
    engine.setUnitSeed(5);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    std::vector<TimedEvent> events {
        { 0.0, { Event::Type::NoteOn, 0, 48, 0.9f } },
        { 0.0, { Event::Type::NoteOn, 0, 60, 0.9f } },
    };
    const auto result = OfflineRenderer::render(engine, events, 1.1);

    auto freqAt = [&](double seconds)
    {
        const int n = 4096;
        const auto spectrum = analyzer::magnitudeSpectrum(
            result.left.data() + static_cast<int>(seconds * 48000.0), n, n);
        return analyzer::peakFrequencyHz(spectrum, 48000.0);
    };

    const double step0 = freqAt(0.08);
    const double step1 = freqAt(0.33);
    const double step2 = freqAt(0.58);
    INFO("steps " << step0 << " " << step1 << " " << step2);
    // 48 then 60 then wraps back to 48: one octave apart.
    REQUIRE(std::abs(step1 / step0 - 2.0) < 0.1);
    REQUIRE(std::abs(step2 / step0 - 1.0) < 0.05);
}

TEST_CASE("Arpeggiator stops when notes are released")
{
    PatchState patch = arpPatch();
    patch.arpMode = 1;
    patch.arpRateHz = 10.0f;

    SynthEngine engine;
    engine.setUnitSeed(9);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    std::vector<TimedEvent> events {
        { 0.0, { Event::Type::NoteOn, 0, 50, 0.9f } },
        { 0.5, { Event::Type::NoteOff, 0, 50, 0.0f } },
    };
    const auto result = OfflineRenderer::render(engine, events, 1.5);
    const int tail = static_cast<int>(result.left.size()) - 9600;
    REQUIRE(analyzer::rms(result.left.data() + tail, 9600) < 1e-4);
}

TEST_CASE("Osc2 FM adds sidebands to a pure carrier")
{
    auto render = [&](float fm)
    {
        PatchState patch = arpPatch();
        patch.osc1.waveform = Waveform::Sine;
        patch.osc2.waveform = Waveform::Sine;
        patch.osc2.semitones = 7;
        patch.osc2.level = 0.0f; // modulator is silent, only modulates
        patch.osc2ToOsc1Fm = fm;
        patch.cutoffHz = 18000.0f;
        SynthEngine engine;
        engine.setUnitSeed(3);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        return OfflineRenderer::renderSingleNote(engine, 60, 0.9f, 0.9, 1.0);
    };

    const double f0 = noteToHz(60);
    const int n = 16384;
    auto residual = [&](const RenderResult& r)
    {
        return analyzer::aliasEnergyDb(
            analyzer::magnitudeSpectrum(r.left.data() + 8000, n, n), 48000.0, f0, 8);
    };

    const double clean = residual(render(0.0f));
    const double modulated = residual(render(0.6f));
    INFO("clean " << clean << " dB, FM " << modulated << " dB");
    // FM sidebands are inharmonic vs the carrier: residual energy jumps.
    REQUIRE(modulated > clean + 10.0);

    for (const float v : render(0.9f).left)
        REQUIRE(std::isfinite(v));
}
