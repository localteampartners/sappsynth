#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

namespace {

PatchState basicPatch()
{
    PatchState patch;
    patch.osc1.level = 1.0f;
    patch.cutoffHz = 8000.0f;
    patch.ampAttack = 0.005f;
    patch.ampRelease = 0.1f;
    return patch;
}

RenderResult renderNote(std::uint64_t seed, const PatchState& patch,
                        int note = 48, double noteSec = 0.5, double totalSec = 1.0)
{
    SynthEngine engine;
    engine.setUnitSeed(seed);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);
    return OfflineRenderer::renderSingleNote(engine, note, 0.8f, noteSec, totalSec);
}

bool identical(const RenderResult& a, const RenderResult& b)
{
    if (a.left.size() != b.left.size())
        return false;
    for (std::size_t i = 0; i < a.left.size(); ++i)
        if (a.left[i] != b.left[i] || a.right[i] != b.right[i])
            return false;
    return true;
}

} // namespace

TEST_CASE("Same unit seed reproduces a render bit-exactly")
{
    const auto patch = basicPatch();
    const auto a = renderNote(1001, patch);
    const auto b = renderNote(1001, patch);
    REQUIRE(identical(a, b));
    REQUIRE(analyzer::rms(a.left.data(), static_cast<int>(a.left.size())) > 0.01);
}

TEST_CASE("Different unit seeds produce different instruments")
{
    PatchState patch = basicPatch();
    patch.characterAmount = 1.0f;
    patch.driftAmountCents = 3.0f;
    const auto a = renderNote(1, patch);
    const auto b = renderNote(2, patch);
    REQUIRE(!identical(a, b));
}

TEST_CASE("Output is always finite and bounded")
{
    PatchState patch = basicPatch();
    patch.resonance = 1.0f;
    patch.mixerDrive = 8.0f;
    patch.filterDriveDb = 24.0f;
    patch.outputDriveDb = 24.0f;
    patch.osc2.level = 1.0f;
    patch.subLevel = 1.0f;
    patch.noiseLevel = 1.0f;

    const auto result = renderNote(7, patch, 36, 0.8, 1.2);
    for (const float v : result.left)
    {
        REQUIRE(std::isfinite(v));
        REQUIRE(std::abs(v) < 4.0f);
    }
}

TEST_CASE("Note events are sample-accurate")
{
    SynthEngine engine;
    engine.setUnitSeed(5);
    PatchState patch = basicPatch();
    patch.ampAttack = 0.001f;
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    const int startSample = 1000; // mid-block offset (1000 % 256 != 0)
    std::vector<TimedEvent> events {
        { startSample / 48000.0, { Event::Type::NoteOn, 0, 60, 1.0f } },
    };
    const auto result = OfflineRenderer::render(engine, events, 0.25);

    int firstAudible = -1;
    for (std::size_t i = 0; i < result.left.size(); ++i)
        if (std::abs(result.left[i]) > 1e-6f)
        {
            firstAudible = static_cast<int>(i);
            break;
        }
    INFO("first audible sample " << firstAudible);
    REQUIRE(firstAudible >= startSample);
    REQUIRE(firstAudible < startSample + 64); // within one control tick
}

TEST_CASE("Repeated notes land on different voice cards (round robin)")
{
    PatchState patch = basicPatch();
    patch.characterAmount = 1.0f;

    SynthEngine engine;
    engine.setUnitSeed(11);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    // Two consecutive identical notes rendered separately must differ (they go
    // through different virtual voice cards + note variation).
    std::vector<TimedEvent> events {
        { 0.0, { Event::Type::NoteOn, 0, 60, 0.8f } },
        { 0.4, { Event::Type::NoteOff, 0, 60, 0.0f } },
        { 0.6, { Event::Type::NoteOn, 0, 60, 0.8f } },
        { 1.0, { Event::Type::NoteOff, 0, 60, 0.0f } },
    };
    const auto result = OfflineRenderer::render(engine, events, 1.4);

    const int sr = 48000;
    const auto* first = result.left.data() + 4800;
    const auto* second = result.left.data() + static_cast<int>(0.6 * sr) + 4800;
    double diff = 0.0;
    for (int i = 0; i < 4800; ++i)
        diff += std::abs(static_cast<double>(first[i]) - static_cast<double>(second[i]));
    REQUIRE(diff > 0.1); // audibly-relevant micro-differences exist
}

TEST_CASE("Polyphony limit steals voices instead of dropping or exploding")
{
    PatchState patch = basicPatch();
    patch.polyphony = 4;

    SynthEngine engine;
    engine.setUnitSeed(3);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);

    std::vector<TimedEvent> events;
    for (int i = 0; i < 12; ++i)
        events.push_back({ i * 0.02, { Event::Type::NoteOn, 0, 40 + i, 0.8f } });
    const auto result = OfflineRenderer::render(engine, events, 0.6);

    REQUIRE(engine.activeVoiceCount() <= 4);
    for (const float v : result.left)
        REQUIRE(std::isfinite(v));
}

TEST_CASE("High quality mode reduces nonlinear aliasing versus Eco")
{
    // Bright note, heavy mixer drive: the nonlinearities alias at 1x and the
    // 4x island should measurably clean it up (architecture §12 exit test).
    PatchState patch = basicPatch();
    patch.mixerDrive = 6.0f;
    patch.cutoffHz = 16000.0f;
    patch.characterAmount = 0.0f;
    patch.driftAmountCents = 0.0f;

    patch.quality = QualityMode::Eco;
    const auto eco = renderNote(9, patch, 91, 0.9, 1.0);
    patch.quality = QualityMode::High;
    const auto high = renderNote(9, patch, 91, 0.9, 1.0);

    const double f0 = noteToHz(91.0);
    const int n = 16384;
    const auto ecoAlias = analyzer::aliasEnergyDb(
        analyzer::magnitudeSpectrum(eco.left.data() + 8000, n, n), 48000.0, f0, 24);
    const auto highAlias = analyzer::aliasEnergyDb(
        analyzer::magnitudeSpectrum(high.left.data() + 8000, n, n), 48000.0, f0, 24);

    INFO("eco " << ecoAlias << " dB, high " << highAlias << " dB");
    REQUIRE(highAlias < ecoAlias - 3.0);
}

TEST_CASE("All notes off silences the engine")
{
    SynthEngine engine;
    engine.setUnitSeed(21);
    engine.setPatch(basicPatch());
    engine.prepare(48000.0, 256);

    std::vector<TimedEvent> events {
        { 0.0, { Event::Type::NoteOn, 0, 50, 0.8f } },
        { 0.1, { Event::Type::NoteOn, 0, 55, 0.8f } },
        { 0.3, { Event::Type::AllNotesOff, 0, 0, 0.0f } },
    };
    const auto result = OfflineRenderer::render(engine, events, 1.5);
    const int tailStart = static_cast<int>(result.left.size()) - 4800;
    REQUIRE(analyzer::rms(result.left.data() + tailStart, 4800) < 1e-4);
}

TEST_CASE("CC 3 clean scales the imperfection sources to the patch's own clean render (v0.12)")
{
    auto render = [](float clean, float character, float drift, float warmup)
    {
        PatchState patch = basicPatch();
        patch.characterAmount = character;
        patch.driftAmountCents = drift;
        patch.warmupAmount = warmup;
        SynthEngine engine;
        engine.setUnitSeed(21);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        engine.setClean(clean);
        return OfflineRenderer::renderSingleNote(engine, 57, 0.8f, 0.6, 0.2);
    };
    const auto cleaned = render(1.0f, 1.0f, 8.0f, 1.0f);
    const auto designedClean = render(0.0f, 0.0f, 0.0f, 0.0f);
    const auto dirty = render(0.0f, 1.0f, 8.0f, 1.0f);
    double diffClean = 0.0, diffDirty = 0.0;
    for (std::size_t i = 0; i < cleaned.left.size(); ++i)
    {
        diffClean += std::abs(double(cleaned.left[i]) - double(designedClean.left[i]));
        diffDirty += std::abs(double(dirty.left[i]) - double(designedClean.left[i]));
    }
    INFO("clean-vs-designed " << diffClean << " dirty-vs-designed " << diffDirty);
    REQUIRE(diffClean < diffDirty * 0.05);
}

TEST_CASE("v0.13 modulation: LFO>PW moves a pulse, sync tears, the wheel adds vibrato, pressure opens the filter")
{
    auto render = [](auto&& tweak, float wheel = 0.0f, float pressure = 0.0f)
    {
        PatchState patch = basicPatch();
        patch.osc1.waveform = Waveform::Pulse;
        patch.osc1.pulseWidth = 0.5f;
        patch.characterAmount = 0.0f;
        patch.driftAmountCents = 0.0f;
        patch.cutoffHz = 1500.0f;
        patch.filterEnvAmount = 0.0f;    // the filter sits where cutoff says, so pressure is measurable
        patch.lfoRateHz = 6.0f;
        tweak(patch);
        SynthEngine engine;
        engine.setUnitSeed(4);
        engine.setPatch(patch);
        engine.prepare(48000.0, 256);
        engine.setModWheel(wheel);
        engine.setAftertouch(pressure);
        return OfflineRenderer::renderSingleNote(engine, 57, 0.8f, 0.8, 0.9);
    };
    auto diff = [](const auto& a, const auto& b) { double d = 0; for (std::size_t i = 0; i < a.left.size(); ++i) d += std::abs(double(a.left[i]) - double(b.left[i])); return d; };
    const auto base = render([](PatchState&) {});
    const auto pwm = render([](PatchState& p) { p.lfoToPw = 0.8f; });
    const auto sync = render([](PatchState& p) { p.oscSync = true; p.osc2.semitones = 7; p.osc2.level = 0.0f; });
    const auto wheel = render([](PatchState&) {}, 1.0f);
    const auto pressed = render([](PatchState&) {}, 0.0f, 1.0f);
    REQUIRE(diff(pwm, base) > 1.0);
    REQUIRE(diff(sync, base) > 1.0);
    REQUIRE(diff(wheel, base) > 1.0);
    // pressure opens the filter: more energy above ~3 kHz. Four cascaded
    // one-pole high-passes (24 dB/oct) so the 220 Hz fundamental cannot leak
    // into the measurement and mask the change.
    auto highBand = [](const std::vector<float>& x)
    {
        const float coef = 1.0f - std::exp(-2.0f * 3.14159f * 3000.0f / 48000.0f);
        float lp[4] { 0, 0, 0, 0 };
        double acc = 0.0;
        for (std::size_t i = 12000; i < x.size(); ++i)
        {
            float v = x[i];
            for (float& st : lp) { st += coef * (v - st); v -= st; }
            acc += std::abs(v);
        }
        return acc;
    };
    const double hfBase = highBand(base.left), hfPressed = highBand(pressed.left);
    INFO("high band base " << hfBase << " pressed " << hfPressed);
    REQUIRE(hfPressed > hfBase * 1.5);
}
