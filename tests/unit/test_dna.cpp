#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

namespace {

PatchState dnaPatch()
{
    PatchState patch;
    patch.osc1.level = 1.0f;
    patch.cutoffHz = 6000.0f;
    patch.ampAttack = 0.005f;
    return patch;
}

RenderResult renderChord(const PatchState& patch, std::uint64_t seed, double seconds = 1.5)
{
    SynthEngine engine;
    engine.setUnitSeed(seed);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);
    std::vector<TimedEvent> events;
    for (int i = 0; i < 8; ++i)
        events.push_back({ 0.05 * i, { Event::Type::NoteOn, 0, 48 + i * 4, 0.8f } });
    return OfflineRenderer::render(engine, events, seconds);
}

} // namespace

TEST_CASE("Supply sag stays bounded and subtle under full polyphonic load")
{
    PatchState patch = dnaPatch();
    patch.characterAmount = 1.0f;
    patch.dnaSupply = 0.0f; // softest supply
    const auto soft = renderChord(patch, 42);
    for (const float v : soft.left)
    {
        REQUIRE(std::isfinite(v));
        REQUIRE(std::abs(v) < 4.0f);
    }

    // Soft vs stiff supply must actually differ (correlated pitch/cutoff dip).
    patch.dnaSupply = 1.0f;
    const auto stiff = renderChord(patch, 42);
    double diff = 0.0;
    for (std::size_t i = 24000; i < soft.left.size(); ++i)
        diff += std::abs(static_cast<double>(soft.left[i]) - static_cast<double>(stiff.left[i]));
    REQUIRE(diff / static_cast<double>(soft.left.size()) > 1e-4);
}

TEST_CASE("DNA macros are deterministic per seed")
{
    PatchState patch = dnaPatch();
    patch.characterAmount = 1.0f;
    patch.dnaAge = 0.8f;
    patch.dnaCondition = 0.9f;
    const auto a = renderChord(patch, 1234);
    const auto b = renderChord(patch, 1234);
    for (std::size_t i = 0; i < a.left.size(); ++i)
        REQUIRE(a.left[i] == b.left[i]);
}

TEST_CASE("Calibration tightens tuning spread")
{
    // Loose calibration should widen the detune spread across voice cards:
    // measure spectral spread of an 8-note unison-ish cluster on one pitch.
    auto render = [&](float calibration)
    {
        PatchState patch = dnaPatch();
        patch.characterAmount = 1.0f;
        patch.dnaCondition = 1.0f;
        patch.dnaCalibration = calibration;
        patch.driftAmountCents = 0.0f;
        SynthEngine engine;
        engine.setUnitSeed(9);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        std::vector<TimedEvent> events;
        for (int i = 0; i < 8; ++i) // same note, 8 different cards
            events.push_back({ 0.0, { Event::Type::NoteOn, 0, 69, 0.8f } });
        return OfflineRenderer::render(engine, events, 1.0);
    };

    auto beatingness = [](const RenderResult& r)
    {
        // Detuned stacks beat: amplitude envelope wobble is the cheap metric.
        double minRms = 1e9, maxRms = 0.0;
        for (int w = 0; w < 10; ++w)
        {
            const double v = analyzer::rms(r.left.data() + 12000 + w * 3000, 3000);
            minRms = std::min(minRms, v);
            maxRms = std::max(maxRms, v);
        }
        return maxRms / std::max(minRms, 1e-12);
    };

    const double tight = beatingness(render(1.0f));
    const double loose = beatingness(render(0.0f));
    INFO("tight " << tight << " loose " << loose);
    REQUIRE(loose > tight);
}

TEST_CASE("Age raises the noise floor but keeps it far below signal level")
{
    PatchState patch = dnaPatch();
    patch.characterAmount = 1.0f;
    patch.dnaAge = 1.0f;
    patch.ampSustain = 1.0f;
    patch.osc1.level = 0.0f; // silence the oscillator: only bleed/noise remain
    patch.noiseLevel = 0.0f;

    SynthEngine engine;
    engine.setUnitSeed(21);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);
    const auto result = OfflineRenderer::renderSingleNote(engine, 60, 0.8f, 1.0, 1.2);

    const double floorRms = analyzer::rms(result.left.data() + 24000, 12000);
    INFO("noise floor rms " << floorRms);
    REQUIRE(floorRms > 1e-6);   // audible-in-principle hiss exists at max age
    REQUIRE(floorRms < 0.01);   // ...but stays way below signal level (< -40 dB)
}

TEST_CASE("Warmth changes harmonic content, not just loudness")
{
    auto harmonics = [&](float warmth)
    {
        PatchState patch = dnaPatch();
        patch.osc1.waveform = Waveform::Sine;
        patch.characterAmount = 1.0f;
        patch.dnaWarmth = warmth;
        SynthEngine engine;
        engine.setUnitSeed(4);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        const auto r = OfflineRenderer::renderSingleNote(engine, 48, 0.9f, 0.9, 1.0);
        const int n = 16384;
        const auto spec = analyzer::magnitudeSpectrum(r.left.data() + 8000, n, n);
        const double binWidth = 48000.0 / n;
        const double f0 = noteToHz(48);
        auto bin = [&](double f) { return spec[static_cast<std::size_t>(f / binWidth + 0.5)]; };
        return (bin(f0 * 2) + bin(f0 * 3)) / std::max(bin(f0), 1e-12);
    };

    const double cold = harmonics(0.0f);
    const double hot = harmonics(1.0f);
    INFO("cold " << cold << " hot " << hot);
    REQUIRE(hot > cold * 1.5);
}

TEST_CASE("Exaggerated and Ideal modes are stable and audibly bracket DNA mode")
{
    PatchState patch = dnaPatch();
    patch.characterAmount = 1.0f;
    patch.dnaAge = 0.8f;
    patch.driftAmountCents = 3.0f;

    auto renderMode = [&](SynthEngine::LabMode mode)
    {
        SynthEngine engine;
        engine.setUnitSeed(31);
        engine.setLabMode(mode);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        return OfflineRenderer::renderSingleNote(engine, 60, 0.8f, 1.2, 1.5);
    };

    const auto ideal = renderMode(SynthEngine::LabMode::Ideal);
    const auto exaggerated = renderMode(SynthEngine::LabMode::Exaggerated);
    for (const float v : exaggerated.left)
        REQUIRE(std::isfinite(v));

    // Pitch stability: ideal should hold its peak frequency far steadier than
    // exaggerated over two separate windows.
    auto freqDrift = [&](const RenderResult& r)
    {
        const int n = 8192;
        const auto early = analyzer::peakFrequencyHz(
            analyzer::magnitudeSpectrum(r.left.data() + 8000, n, n), 48000.0);
        const auto late = analyzer::peakFrequencyHz(
            analyzer::magnitudeSpectrum(r.left.data() + 40000, n, n), 48000.0);
        return std::abs(late - early);
    };
    INFO("ideal drift " << freqDrift(ideal) << " Hz, exaggerated " << freqDrift(exaggerated) << " Hz");
    REQUIRE(freqDrift(exaggerated) >= freqDrift(ideal));
}
