// preset-audit — render every factory preset and report its true level.
//
// Presets are written by hand, and a patch that stacks two oscillators, a
// sub, unison and drive can easily leave the plugin clipping before the user
// touches anything. This loads each preset into the real processor, plays a
// chord, and measures peak / RMS so the bank can be levelled by measurement
// instead of guesswork.
//
//   preset-audit             report every preset, exit 1 if any is too hot
//   preset-audit --trims     print the master-dB trim each preset needs
//
// The ceiling is peak headroom, not loudness: a synth should leave room for
// the rest of the mix and never clip a chord at full velocity.

#include <juce_audio_processors/juce_audio_processors.h>

#include "FactoryPresets.h"
#include "../../source/parameters/ParameterIds.h"
#include "PluginProcessor.h"
#include "../../source/engine/PresetPatch.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

// Presets span basses, leads and pads, so one test signal cannot represent
// them all: a mono bass with a 300 Hz filter is nearly silent under a C4
// chord, and calibrating from that would boost it into distortion when
// actually played low. Each preset is measured across four passes and the
// loudest wins — the worst case a player can actually hit.
//
// The chord pass is EIGHT notes at full velocity, not four (issue #1). Voices
// sum on a bus with no headroom scaling, so peak keeps growing with the note
// count: calibrating on a four-note chord left 10 of 186 presets above the
// ship ceiling when eight notes were held, and one of them reached full scale.
// Eight is the default polyphony — the realistic worst case a player hits.
struct TestPass { int notes[8]; int count; };
constexpr TestPass kPasses[] = {
    { { 36,  0,  0,  0,  0,  0,  0,  0 }, 1 },   // low single note (bass register)
    { { 48,  0,  0,  0,  0,  0,  0,  0 }, 1 },   // mid single note
    { { 60,  0,  0,  0,  0,  0,  0,  0 }, 1 },   // upper single note (leads)
    { { 36, 43, 48, 52, 55, 60, 64, 67 }, 8 },   // eight-note chord (full polyphony)
};

constexpr float kTargetPeakDb = -6.0f;   // where a preset should sit
constexpr float kCeilingDb    = -3.0f;   // above this it is too hot to ship

struct Measurement { float peak = 0.0f; float rms = 0.0f; };

Measurement renderOnePass (sappsynth::SappSynthProcessor& processor, int index,
                           const TestPass& pass, int startCard)
{
    processor.applyFactoryPreset (index);
    // Park the voice allocator. Each of the 16 cards carries its own gain and
    // pan tolerances, so the same chord measures up to 3.5 dB apart depending
    // on which cards it lands on. Without this the audit inherits whatever
    // cursor the previous preset left, which made every number depend on the
    // ORDER of the bank — insert a preset and the ones after it re-measure.
    processor.synthEngine().resetVoiceAllocation (startCard);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    // Let the preset-switch discontinuity decay BEFORE the note starts.
    // (Skipping the first 200 ms of the note instead would miss short
    // percussive patches entirely — a 120 ms pluck would measure as silence
    // and get boosted into distortion.)
    for (int block = 0; block < (int) (0.2 * kSampleRate / kBlockSize); ++block)
    {
        buffer.clear();
        juce::MidiBuffer none;
        processor.processBlock (buffer, none);
    }

    for (int n = 0; n < pass.count; ++n)
        midi.addEvent (juce::MidiMessage::noteOn (1, pass.notes[n], 1.0f), 0);

    Measurement result;
    double sumSquares = 0.0;
    juce::int64 frames = 0;

    // ~2.5 s held, then ~1.5 s of release tail, all of it measured.
    const int heldBlocks    = (int) (2.5 * kSampleRate / kBlockSize);
    const int releaseBlocks = (int) (1.5 * kSampleRate / kBlockSize);


    for (int block = 0; block < heldBlocks + releaseBlocks; ++block)
    {
        if (block == heldBlocks)
        {
            midi.clear();
            for (int n = 0; n < pass.count; ++n)
                midi.addEvent (juce::MidiMessage::noteOff (1, pass.notes[n]), 0);
        }
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float sample = std::abs (data[i]);
                result.peak = juce::jmax (result.peak, sample);
                sumSquares += double (sample) * double (sample);
                ++frames;
            }
        }
    }
    result.rms = frames > 0 ? (float) std::sqrt (sumSquares / (double) frames) : 0.0f;
    return result;
}

// Card positions the chord pass is measured from. Sweeping all 16 would take
// four times as long for ~0.3 dB more worst case; these four cover the spread.
constexpr int kChordStartCards[] = { 0, 4, 8, 12 };

Measurement renderPreset (sappsynth::SappSynthProcessor& processor, int index)
{
    Measurement worst;
    for (const auto& pass : kPasses)
    {
        if (pass.count == 1)
        {
            const auto m = renderOnePass (processor, index, pass, 0);
            if (m.peak > worst.peak) worst = m;
            continue;
        }
        // A chord spans several cards, so the allocation it starts from is what
        // decides its peak. Calibrate against the worst one a player can hit,
        // not against whichever one this run happened to produce.
        for (int startCard : kChordStartCards)
        {
            const auto m = renderOnePass (processor, index, pass, startCard);
            if (m.peak > worst.peak) worst = m;
        }
    }
    return worst;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    bool trimsOnly = false, defaultsOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--trims")    trimsOnly = true;
        if (juce::String (argv[i]) == "--defaults") defaultsOnly = true;
    }

    sappsynth::SappSynthProcessor processor;
    processor.setRateAndBufferSizeDetails (kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);

    if (defaultsOnly)
    {
        // The unit suite renders the factory bank without JUCE, using
        // sappsynth::defaultPatch() as the starting point. That is a hand copy
        // of the APVTS defaults, so prove the two still agree — a changed
        // parameter default would otherwise make every core-side level
        // measurement quietly wrong.
        const auto* difference = sappsynth::firstPatchDifference (
            processor.buildPatchFromParameters(), sappsynth::defaultPatch());
        if (difference != nullptr)
        {
            std::printf ("defaultPatch() disagrees with the APVTS defaults at: %s\n"
                         "Update defaultPatch() in source/engine/PresetPatch.cpp.\n",
                         difference);
            return 1;
        }
        std::printf ("defaultPatch() matches the APVTS defaults\n");
        return 0;
    }

    const auto& bank = sappsynth::presets::all();
    int tooHot = 0, clipping = 0;

    if (! trimsOnly)
        std::printf ("%-24s %-10s %8s %8s\n", "preset", "category", "peak dB", "rms dB");

    for (size_t i = 0; i < bank.size(); ++i)
    {
        const auto m = renderPreset (processor, (int) i);
        const float peakDb = juce::Decibels::gainToDecibels (m.peak, -120.0f);
        const float rmsDb  = juce::Decibels::gainToDecibels (m.rms, -120.0f);

        if (m.peak >= 0.999f) ++clipping;
        if (peakDb > kCeilingDb) ++tooHot;

        if (trimsOnly)
        {
            // name | measured peak | the master this preset rendered with, so
            // the calibration can compute an absolute new master value.
            const float master =
                processor.apvts.getRawParameterValue (sappsynth::param::master)->load();
            std::printf ("%s|%.2f|%.2f\n", bank[i].name, peakDb, master);
        }
        else
        {
            std::printf ("%-24s %-10s %8.1f %8.1f%s\n", bank[i].name, bank[i].category,
                         peakDb, rmsDb,
                         m.peak >= 0.999f ? "  CLIPPING"
                                          : (peakDb > kCeilingDb ? "  hot" : ""));
        }
    }

    if (! trimsOnly)
        std::printf ("\n%d presets, %d above %.0f dBFS, %d clipping\n",
                     (int) bank.size(), tooHot, kCeilingDb, clipping);
    return tooHot > 0 ? 1 : 0;
}
