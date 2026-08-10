// Polyphony headroom regressions (issue #1).
//
// Voices sum onto a bus with no headroom scaling, so a patch's peak keeps
// growing with the number of held notes. The factory bank is levelled by
// measurement, but it used to be levelled against a FOUR-note chord: hold eight
// and 10 of the 186 presets went over the -3 dBFS ship ceiling, one of them all
// the way to full scale. The bank is now calibrated at full polyphony, and this
// file is what keeps it that way — it fails on the four-note calibration.
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include "engine/PresetPatch.h"
#include "engine/SynthEngine.h"
#include "engine/VoiceManager.h"
#include "lab/OfflineRenderer.h"

using namespace sappsynth;

namespace {

// preset-audit is the exhaustive gate: it renders through the real processor,
// levels the bank to -6 dBFS, and sweeps the voice allocator. This suite is the
// fast guard, with shorter renders and no release tail, but it holds the same
// -3 dBFS ship ceiling. On the four-note calibration five presets go over it
// and the hottest reaches -2.0 dBFS; on the current bank the hottest is
// -4.5 dBFS.
constexpr float kCeilingDb = -3.0f;

// The seed a fresh plugin instance runs (PluginProcessor's constructor). Voice
// tolerances follow from it, so measuring on any other unit measures a
// different instrument.
constexpr std::uint64_t kStockUnitSeed = 0x5A995EEDull;

float toDb(float gain) { return gain > 1e-9f ? 20.0f * std::log10(gain) : -180.0f; }

struct Level { float peakDb; float rmsDb; };

Level measure(const PatchState& patch, const std::vector<int>& notes, double seconds,
              int startCard = 0)
{
    SynthEngine engine;
    engine.setUnitSeed(kStockUnitSeed);
    engine.setPatch(patch);
    engine.prepare(48000.0, 256);
    engine.resetVoiceAllocation(startCard);

    std::vector<TimedEvent> events;
    for (int note : notes)
        events.push_back({ 0.0, Event { Event::Type::NoteOn, 0, note, 1.0f } });

    const auto rendered = OfflineRenderer::render(engine, events, seconds, 256);
    float peak = 0.0f;
    double sumSquares = 0.0;
    for (std::size_t i = 0; i < rendered.left.size(); ++i)
    {
        peak = std::max({ peak, std::abs(rendered.left[i]), std::abs(rendered.right[i]) });
        sumSquares += double(rendered.left[i]) * rendered.left[i]
                    + double(rendered.right[i]) * rendered.right[i];
    }
    const double frames = double(rendered.left.size()) * 2.0;
    return { toDb(peak), toDb(float(std::sqrt(sumSquares / frames))) };
}

// Eight notes at full velocity: the default polyphony, and the realistic worst
// case. A long-attack pad needs proportionally longer to reach its peak.
const std::vector<int>& fullPolyphonyChord()
{
    static const std::vector<int> notes { 36, 43, 48, 52, 55, 60, 64, 67 };
    return notes;
}

double settleSeconds(const PatchState& patch)
{
    return std::max(0.9, double(patch.ampAttack) + 0.45);
}

const presets::Preset& presetNamed(const char* name)
{
    for (const auto& preset : presets::all())
        if (std::strcmp(preset.name, name) == 0)
            return preset;
    FAIL("no factory preset named " << name);
    return presets::all().front();
}

} // namespace

TEST_CASE("Every factory preset stays under the ceiling at full polyphony")
{
    const char* worstName = "";
    float worstDb = -180.0f;
    int over = 0;

    for (const auto& preset : presets::all())
    {
        const auto patch = patchForPreset(preset);
        const auto level = measure(patch, fullPolyphonyChord(), settleSeconds(patch));
        if (level.peakDb > kCeilingDb)
            ++over;
        if (level.peakDb > worstDb)
        {
            worstDb = level.peakDb;
            worstName = preset.name;
        }
    }

    INFO(over << " preset(s) over " << kCeilingDb << " dBFS; hottest is "
              << worstName << " at " << worstDb << " dBFS");
    CHECK(over == 0);
}

TEST_CASE("The hottest pads stay under the ceiling from any voice-card position")
{
    // The 16 voice cards carry per-card gain and pan tolerances, so which ones
    // a chord lands on is worth up to 3.5 dB. The allocator is round-robin:
    // where the cursor sits depends only on how many notes were played before,
    // which the player does not control and the calibration must not assume.
    for (const char* name : { "Dark Cathedral", "Glacier Pad", "Metal Drone", "Warm Tape Pad" })
    {
        const auto patch = patchForPreset(presetNamed(name));
        const double seconds = settleSeconds(patch);

        float worstDb = -180.0f;
        int worstCard = 0;
        for (int card = 0; card < VoiceManager::kMaxVoices; ++card)
        {
            const auto level = measure(patch, fullPolyphonyChord(), seconds, card);
            if (level.peakDb > worstDb)
            {
                worstDb = level.peakDb;
                worstCard = card;
            }
        }

        INFO(name << ": worst card position " << worstCard << " at " << worstDb << " dBFS");
        CHECK(worstDb <= kCeilingDb);
    }
}

TEST_CASE("The factory bank and the engine agree on every parameter ID")
{
    // patchForPreset() is a second copy of the APVTS -> PatchState mapping that
    // lets this suite render the bank without JUCE. If a preset gains an ID the
    // core does not know, every level measured above is silently wrong.
    for (const auto& preset : presets::all())
    {
        PatchState patch = defaultPatch();
        INFO("preset: " << preset.name);
        CHECK(applyPresetValues(patch, preset.values));
    }
}

TEST_CASE("Holding more notes raises the level, and the bank is levelled for it")
{
    // Two things at once. Peak must GROW with the note count — a per-voice gain
    // that fell as voices were added would hold the level flat and make chords
    // quieter than single notes, which is not how a mixing bus behaves. And the
    // level it grows to is what the bank has to be calibrated against, which is
    // the mistake a four-note calibration made.
    for (const char* name : { "Dark Cathedral", "Glacier Pad", "Warm Tape Pad" })
    {
        const auto patch = patchForPreset(presetNamed(name));
        const double seconds = settleSeconds(patch);

        const auto one = measure(patch, { 60 }, seconds);
        const auto four = measure(patch, { 48, 55, 60, 64 }, seconds);
        const auto eight = measure(patch, fullPolyphonyChord(), seconds);

        INFO(name << ": 1 note " << one.peakDb << ", 4 notes " << four.peakDb
                  << ", 8 notes " << eight.peakDb << " dBFS");
        CHECK(four.peakDb > one.peakDb);
        CHECK(eight.peakDb > four.peakDb - 1.0f);
        CHECK(eight.peakDb <= kCeilingDb);
    }
}

TEST_CASE("Mix Drive reaches unity and below instead of only boosting")
{
    // The parameter used to run 1..8 with a default of 1.2, so the mixer
    // saturator could not be switched off and every patch shipped with it
    // already engaged. 1.0 is the default and the transparent setting now, and
    // the range reaches down to 0.25.
    PatchState quiet = defaultPatch();
    quiet.masterDb = 0.0f;
    PatchState unity = quiet;
    PatchState hot = quiet;
    quiet.mixerDrive = 0.25f;
    unity.mixerDrive = 1.0f;
    hot.mixerDrive = 4.0f;

    CHECK(defaultPatch().mixerDrive == 1.0f);

    const auto quietLevel = measure(quiet, { 60 }, 0.9);
    const auto unityLevel = measure(unity, { 60 }, 0.9);
    const auto hotLevel = measure(hot, { 60 }, 0.9);

    INFO("0.25 -> " << quietLevel.rmsDb << ", 1.0 -> " << unityLevel.rmsDb
                    << ", 4.0 -> " << hotLevel.rmsDb << " dBFS RMS");
    CHECK(quietLevel.rmsDb < unityLevel.rmsDb - 3.0f);
    CHECK(hotLevel.rmsDb > unityLevel.rmsDb);
}
