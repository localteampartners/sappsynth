// Gain-staging regressions (issue #2).
//
// Four hidden gain stages, measured while closing issue #1 and fixed in
// v0.11.0. One TEST_CASE per fault, each written to fail on the pre-0.11 DSP:
//
//   1. the reverb's wet path was fed at a fixed 0.25 with no normalisation, so
//      it ran +17 to +30 dB above the dry signal and `Size` was a loudness
//      control with a tail attached;
//   2. all three effect Mix controls ADDED wet on top of an untouched dry
//      signal, so Mix could only ever raise the level;
//   3. the output drive normalised its tanh at full scale, applying +1.48 dB
//      to everything below it while claiming to be transparent;
//   4. voices summed onto a bus with no headroom, so a chord's peak stopped
//      growing at six notes — the engine soft-clipping itself.
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>
#include "dsp/effects/Chorus.h"
#include "dsp/effects/Delay.h"
#include "dsp/effects/Reverb.h"
#include "dsp/nonlinear/OutputStage.h"
#include "engine/PresetPatch.h"
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"

using namespace sappsynth;

namespace {

constexpr double kSr = 48000.0;
constexpr std::uint64_t kStockUnitSeed = 0x5A995EEDull;

float toDb(float gain) { return gain > 1e-9f ? 20.0f * std::log10(gain) : -180.0f; }

std::vector<float> broadband(int n, unsigned seed = 7)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    std::vector<float> x(static_cast<std::size_t>(n));
    for (auto& v : x)
        v = dist(rng);
    return x;
}

float peakOf(const std::vector<float>& x, int from = 0, int to = -1)
{
    if (to < 0)
        to = static_cast<int>(x.size());
    float p = 0.0f;
    for (int i = from; i < to; ++i)
        p = std::max(p, std::abs(x[static_cast<std::size_t>(i)]));
    return p;
}

float rmsOf(const std::vector<float>& x, int from = 0, int to = -1)
{
    if (to < 0)
        to = static_cast<int>(x.size());
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += double(x[static_cast<std::size_t>(i)]) * x[static_cast<std::size_t>(i)];
    return static_cast<float>(std::sqrt(sum / std::max(1, to - from)));
}

// Run an effect over the same broadband input at one Mix setting.
template <typename Configure>
std::vector<float> atMix(Configure configure, float mix, const std::vector<float>& input)
{
    auto left = input, right = input;
    configure(mix, left, right, static_cast<int>(input.size()));
    return left;
}

const presets::Preset& presetNamed(const char* name)
{
    for (const auto& preset : presets::all())
        if (std::strcmp(preset.name, name) == 0)
            return preset;
    FAIL("no factory preset named " << name);
    return presets::all().front();
}

// Bare patch: the preset's voice, no effects, Master at 0 — what the bus does
// before anything downstream can hide it.
PatchState barePatch(const char* name)
{
    auto patch = patchForPreset(presetNamed(name));
    patch.masterDb = 0.0f;
    patch.chorusMix = patch.delayMix = patch.reverbMix = 0.0f;
    return patch;
}

float renderPeakDb(const PatchState& patch, const std::vector<int>& notes, double seconds)
{
    SynthEngine engine;
    engine.setUnitSeed(kStockUnitSeed);
    engine.setPatch(patch);
    engine.prepare(kSr, 256);
    engine.resetVoiceAllocation(0);

    std::vector<TimedEvent> events;
    for (int note : notes)
        events.push_back({ 0.0, Event { Event::Type::NoteOn, 0, note, 1.0f } });

    const auto rendered = OfflineRenderer::render(engine, events, seconds, 256);
    float peak = 0.0f;
    for (std::size_t i = 0; i < rendered.left.size(); ++i)
        peak = std::max({ peak, std::abs(rendered.left[i]), std::abs(rendered.right[i]) });
    return toDb(peak);
}

// Time for an impulse response to fall 60 dB below its loudest 50 ms window.
double decayToMinus60(Reverb& reverb, int seconds)
{
    const int n = static_cast<int>(kSr) * seconds;
    std::vector<float> left(static_cast<std::size_t>(n), 0.0f), right = left;
    left[0] = right[0] = 1.0f;
    reverb.process(left.data(), right.data(), n);

    const int window = 2400;
    std::vector<float> envelope;
    float loudest = 0.0f;
    for (int i = 0; i + window <= n; i += window)
    {
        envelope.push_back(rmsOf(left, i, i + window));
        loudest = std::max(loudest, envelope.back());
    }
    for (std::size_t i = 0; i < envelope.size(); ++i)
        if (envelope[i] < loudest * 0.001f)
            return double(i) * window / kSr;
    return double(seconds);
}

} // namespace

TEST_CASE("Effect Mix crossfades between dry and wet instead of adding wet on top")
{
    // Three properties, and the old DSP broke at least one in every effect:
    //
    //   * Mix 0 is bit-exact dry (all three passed this).
    //   * Mix 1 is fully WET — the dry signal is gone. The old Delay left the
    //     dry at full level at Mix 1 (0.0 dB of leakage), Reverb -3.1 dB and
    //     Chorus -6.0 dB, all of it audible.
    //   * The output is linear in Mix, so out(m) is exactly the crossfade of
    //     out(0) and out(1). The old Reverb and Chorus were QUADRATIC in mix
    //     (`mix * (wet - dry * k * mix)`), which is the shape that let the
    //     control raise the total level.
    const auto input = broadband(static_cast<int>(kSr));

    struct Case
    {
        const char* name;
        void (*configure)(float, std::vector<float>&, std::vector<float>&, int);
        int dryWindow;   // samples before any wet can arrive
    };

    const Case cases[] {
        { "Chorus",
          [](float mix, std::vector<float>& l, std::vector<float>& r, int n) {
              Chorus fx; fx.prepare(kSr); fx.setParameters(mix, 1.0f);
              fx.process(l.data(), r.data(), n); },
          600 },       // shortest chorus tap is 14 ms
        { "Delay",
          [](float mix, std::vector<float>& l, std::vector<float>& r, int n) {
              StereoDelay fx; fx.prepare(kSr); fx.setParameters(0.35f, 0.35f, mix);
              fx.process(l.data(), r.data(), n); },
          1400 },      // delay time is 350 ms
        { "Reverb",
          [](float mix, std::vector<float>& l, std::vector<float>& r, int n) {
              Reverb fx; fx.prepare(kSr); fx.setParameters(0.5f, mix);
              fx.process(l.data(), r.data(), n); },
          1100 },      // shortest comb is 1116 samples
    };

    for (const auto& fx : cases)
    {
        INFO("effect: " << fx.name);
        const auto dry = atMix(fx.configure, 0.0f, input);
        const auto wet = atMix(fx.configure, 1.0f, input);

        // Mix 0 leaves the signal alone.
        for (std::size_t i = 0; i < input.size(); ++i)
            REQUIRE(dry[i] == input[i]);

        // Mix 1 is fully wet: nothing of the dry signal survives in the window
        // before the effect's own delay can deliver anything.
        const float leakDb = toDb(peakOf(wet, 0, fx.dryWindow) / peakOf(input, 0, fx.dryWindow));
        INFO("dry leakage at Mix 1: " << leakDb << " dB");
        CHECK(leakDb < -60.0f);

        // ...and every setting in between is the exact crossfade.
        float worstError = 0.0f;
        float loudestDb = -180.0f;
        for (float mix : { 0.25f, 0.5f, 0.75f })
        {
            const auto out = atMix(fx.configure, mix, input);
            for (std::size_t i = 0; i < input.size(); ++i)
                worstError = std::max(worstError,
                                      std::abs(out[i] - ((1.0f - mix) * dry[i] + mix * wet[i])));
            loudestDb = std::max(loudestDb, toDb(peakOf(out)));
        }
        INFO("worst crossfade error " << worstError);
        CHECK(worstError < 1.0e-5f);

        // A crossfade cannot invent level: no intermediate setting is louder
        // than the louder of its two endpoints.
        const float endpointsDb = std::max(toDb(peakOf(dry)), toDb(peakOf(wet)));
        INFO("loudest intermediate " << loudestDb << " dB vs endpoints " << endpointsDb << " dB");
        CHECK(loudestDb <= endpointsDb + 0.1f);
    }
}

TEST_CASE("Reverb Size buys tail length, not level")
{
    // Before v0.11 the comb bank was fed at a fixed 0.25 with no normalisation
    // against feedback, so the full-wet signal measured +18.5 to +21.8 dB RMS
    // above the dry one and moved 3.3 dB across the Size range. Size raised the
    // level and the tail together; players used it as a send.
    float quietest = 200.0f, loudest = -200.0f;
    for (float size : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        Reverb reverb;
        reverb.prepare(kSr);
        reverb.setParameters(size, 1.0f);

        const auto input = broadband(static_cast<int>(kSr) * 2, 21);
        auto left = input, right = input;
        reverb.process(left.data(), right.data(), static_cast<int>(input.size()));

        const float wetDb = toDb(rmsOf(left) / rmsOf(input));
        INFO("size " << size << ": full-wet RMS " << wetDb << " dB re dry");
        // The wet path sits at the dry signal's loudness, not above it.
        CHECK(std::abs(wetDb) < 3.0f);
        quietest = std::min(quietest, wetDb);
        loudest = std::max(loudest, wetDb);
    }
    INFO("Size moves the full-wet level by " << (loudest - quietest) << " dB");
    CHECK(loudest - quietest < 2.0f);

    // What Size DOES move is the tail.
    Reverb small, large;
    small.prepare(kSr);
    large.prepare(kSr);
    small.setParameters(0.0f, 1.0f);
    large.setParameters(1.0f, 1.0f);
    const double shortTail = decayToMinus60(small, 14);
    const double longTail = decayToMinus60(large, 14);
    INFO("decay to -60 dB: size 0 = " << shortTail << " s, size 1 = " << longTail << " s");
    CHECK(longTail > shortTail * 4.0);
}

TEST_CASE("The output drive is exactly unity at unity and an honest gain above it")
{
    // The old stage normalised tanh() at FULL SCALE:
    //     norm = 1 / fastTanh(max(d, 1) * 0.8);
    //     y    = fastTanh(x * d * 0.8) * norm;
    // Small-signal gain at d = 1 is 0.8 * 1.506 = +1.48 dB, applied to
    // everything below full scale, and 0 -> 6 dB of drive moved a quiet signal
    // by only 3.1 dB. Neither is true of a soft knee.

    // Bit-exact identity below the knee — not "within a tolerance".
    for (int i = 0; i <= 1000; ++i)
    {
        const float x = -kOutputKnee + 2.0f * kOutputKnee * float(i) / 1000.0f;
        REQUIRE(outputStage(x, 1.0f) == x);
    }

    // Drive is a gain in dB, and reads as one on a signal that does not need
    // limiting.
    const float quiet = 0.01f;   // -40 dBFS
    for (float driveDb : { 0.0f, 6.0f, 12.0f, 24.0f })
    {
        const float measured = toDb(outputStage(quiet, dbToGain(driveDb)) / quiet);
        INFO("drive " << driveDb << " dB measures " << measured << " dB");
        CHECK(std::abs(measured - driveDb) < 0.01f);
    }

    // Above the knee it limits, monotonically, and never leaves full scale.
    // fastTanh() reaches exactly 1 at 3, so the knee pins at full scale from
    // +6 dBFS in — that is the limiter, and it is where it belongs.
    float previous = 0.0f;
    for (int i = 1; i <= 400; ++i)
    {
        const float x = float(i) * 0.05f;
        const float y = softKnee(x);
        REQUIRE(y >= previous);
        REQUIRE(y <= 1.0f);
        if (x < 2.0f)
            REQUIRE(y > previous);
        previous = y;
    }
    CHECK(softKnee(1.0f) > 0.85f);   // 0 dBFS in is barely bent, not squashed

    // Same claim measured through the whole engine, where the fault lived: on
    // a signal well below the knee, Output Drive moves the level by exactly
    // what it says.
    auto patch = barePatch("Warm Tape Pad");
    patch.masterDb = -24.0f;
    const float at0 = renderPeakDb(patch, { 60 }, 1.2);
    patch.outputDriveDb = 6.0f;
    const float at6 = renderPeakDb(patch, { 60 }, 1.2);
    INFO("engine: drive 0 dB -> " << at0 << " dBFS, drive 6 dB -> " << at6 << " dBFS");
    CHECK(std::abs((at6 - at0) - 6.0f) < 0.2f);
}

TEST_CASE("A chord's peak keeps growing with polyphony instead of flattening")
{
    // The engine used to soft-clip itself: with Master at 0 and no effects,
    // Dark Cathedral measured -2.29 dBFS at one voice and then +3.39, +3.41,
    // +3.41, +3.42 at five, six, seven and eight — flat, because the output
    // drive's tanh was holding it there. Fifteen dB of distortion on every
    // dense chord, before anything downstream saw it.
    for (const char* name : { "Dark Cathedral", "Glacier Pad" })
    {
        const auto patch = barePatch(name);
        const double seconds = std::max(0.9, double(patch.ampAttack) + 0.45);
        const int chord[] { 36, 43, 48, 52, 55, 60, 64, 67 };

        std::vector<int> notes;
        std::vector<float> curve;
        for (int note : chord)
        {
            notes.push_back(note);
            curve.push_back(renderPeakDb(patch, notes, seconds));
        }

        INFO(name << ": 1..8 voices = " << curve[0] << ", " << curve[1] << ", " << curve[2]
                  << ", " << curve[3] << ", " << curve[4] << ", " << curve[5] << ", "
                  << curve[6] << ", " << curve[7] << " dBFS");

        for (std::size_t i = 1; i < curve.size(); ++i)
            CHECK(curve[i] > curve[i - 1]);

        // The flat region was 6 voices and up. That is where the old curve
        // moved 0.01 dB per added note.
        CHECK(curve[7] - curve[5] > 1.0f);
        CHECK(curve[5] - curve[3] > 1.5f);
    }
}
