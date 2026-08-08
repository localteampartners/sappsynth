// sapp-uishot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappUiShot [output.png]
//   SappUiShot --cctest       SappLink CC-in proof
//   SappUiShot --presettest   user preset round-trip proof (sapplink/PRESETS.md)

#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include "PluginProcessor.h"
#include "UserPresets.h"
#include "parameters/ParameterIds.h"
#include "lab/SignalAnalyzer.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SappUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    // --cctest: end-to-end SappLink proof — drive CC 74 through processBlock
    // and verify the rendered spectrum brightens (cutoff actually moved).
    void runCcTest()
    {
        sappsynth::SappSynthProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        std::vector<float> output;

        const int blocks = 140; // ~1.5 s
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 36, 0.9f), 0);
            // First half: CC74 low (dark). Second half: CC74 high (bright).
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, b < blocks / 2 ? 25 : 118), 0);
            buffer.clear();
            proc.processBlock(buffer, midi);
            const float* l = buffer.getReadPointer(0);
            output.insert(output.end(), l, l + 512);
        }

        auto centroid = [&](int start)
        {
            const int n = 8192;
            const auto spec = sappsynth::analyzer::magnitudeSpectrum(output.data() + start, n, n);
            double num = 0, den = 0;
            for (std::size_t i = 1; i < spec.size(); ++i)
            {
                num += spec[i] * static_cast<double>(i);
                den += spec[i];
            }
            return num / std::max(den, 1e-12) * (48000.0 / n);
        };

        const double dark = centroid(512 * 25);          // settled in low-CC half
        const double bright = centroid(512 * 125);       // settled in high-CC half
        const bool pass = bright > dark * 2.0;
        std::printf("CC74 sweep: centroid dark %.0f Hz -> bright %.0f Hz  [%s]\n",
                    dark, bright, pass ? "PASS" : "FAIL");
        setApplicationReturnValue(pass ? 0 : 1);
        quit();
    }

    // --presettest: the preset-system proof (sapplink/PRESETS.md). Saves the
    // live parameter state as a user preset, reloads it into a FRESH
    // processor, and checks every parameter matches bit-for-bit — plus the
    // regressions the preset work must not break (program change, the `preset`
    // choice parameter, host state save/load).
    void runPresetTest()
    {
        using namespace sappsynth;
        namespace up = sapp::userpresets;

        auto check = [this](bool ok, const char* what) {
            std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
            if (!ok) ++presetFailures;
        };
        auto normalisedState = [](juce::AudioProcessor& p) {
            std::map<juce::String, float> values;
            for (auto* parameter : p.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
                    values[withId->paramID] = withId->getValue();
            return values;
        };

        const auto dir = up::presetDir(SappSynthProcessor::kInstrument);
        std::printf("preset dir: %s\n", dir.getFullPathName().toRawUTF8());

        // --- 1. capture a sound -------------------------------------------
        SappSynthProcessor source;
        source.applyFactoryPreset(1);                       // "Acid Bass"
        auto* cutoff = source.apvts.getParameter(param::cutoff);
        auto* release = source.apvts.getParameter(param::ampRelease);
        cutoff->setValueNotifyingHost(0.312345f);
        release->setValueNotifyingHost(0.876543f);
        const float cutoffHz = cutoff->convertFrom0to1(cutoff->getValue());
        std::printf("captured: cutoff %.4f norm (%.2f Hz), amp release %.4f norm\n",
                    cutoff->getValue(), cutoffHz, release->getValue());

        juce::String error;
        check(source.saveUserPreset("RoundTrip Test", "written by --presettest", error),
              "saveUserPreset wrote a file");
        if (error.isNotEmpty()) std::printf("      %s\n", error.toRawUTF8());

        const auto file = dir.getChildFile("RoundTrip Test.json");
        check(file.existsAsFile(), "the preset file exists on disk");
        std::printf("--- %s ---\n%s\n", file.getFileName().toRawUTF8(),
                    file.loadFileAsString().substring(0, 400).toRawUTF8());

        // The JSON must describe the parameters we actually have.
        up::UserPreset parsed;
        juce::String parseError;
        check(up::parse(file, SappSynthProcessor::kInstrument, parsed, parseError),
              "the file parses back as a sapplink v1 preset");
        const auto captured = normalisedState(source);
        int mismatchedInFile = 0;
        for (const auto& [id, value] : parsed.params)
            if (captured.count(id) == 0 || captured.at(id) != value)
                ++mismatchedInFile;
        std::printf("params in file: %d, params on the processor (minus `preset`): %d, mismatched: %d\n",
                    (int) parsed.params.size(), (int) captured.size() - 1, mismatchedInFile);
        check(mismatchedInFile == 0, "every value in the JSON equals the live parameter value");
        check((int) parsed.params.size() == (int) captured.size() - 1,
              "the file lists every parameter except `preset`");

        // --- 2. round-trip into a FRESH instance --------------------------
        SappSynthProcessor fresh;
        check(fresh.loadUserPreset("RoundTrip Test", error), "a fresh instance loads it by name");
        const auto restored = normalisedState(fresh);
        float maxDiff = 0.0f;
        juce::String worst;
        for (const auto& [id, value] : captured)
        {
            if (id == up::kPresetParamId) continue;   // the chooser is not the sound
            const float diff = std::abs(restored.at(id) - value);
            if (diff > maxDiff) { maxDiff = diff; worst = id; }
        }
        std::printf("max normalised difference across %d parameters: %.9g (%s)\n",
                    (int) captured.size() - 1, maxDiff, worst.toRawUTF8());
        check(maxDiff == 0.0f, "every parameter round-trips EXACTLY");

        // --- 3. the `preset` choice parameter actually changes the sound ---
        auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            fresh.apvts.getParameter(up::kPresetParamId));
        check(choice != nullptr, "the `preset` parameter exists and is a choice parameter");
        if (choice != nullptr)
        {
            std::printf("`preset` choices: %d (factory %d, user %d), first=\"%s\" last=\"%s\"\n",
                        choice->choices.size(), fresh.factoryPresetCount(),
                        choice->choices.size() - fresh.factoryPresetCount(),
                        choice->choices[0].toRawUTF8(),
                        choice->choices[choice->choices.size() - 1].toRawUTF8());
            check(choice->choices.size() >= fresh.factoryPresetCount() + 1,
                  "the choice list carries the user preset too");

            auto* fCutoff = fresh.apvts.getParameter(param::cutoff);
            auto* fRes = fresh.apvts.getParameter(param::resonance);
            const float before[2] = { fCutoff->getValue(), fRes->getValue() };
            fresh.applyPresetChoice(32);   // "Dark Cathedral"
            const float after[2] = { fCutoff->getValue(), fRes->getValue() };
            std::printf("applyPresetChoice(32) -> cutoff %.6f -> %.6f, resonance %.6f -> %.6f\n",
                        before[0], after[0], before[1], after[1]);
            check(before[0] != after[0] || before[1] != after[1],
                  "selecting a factory preset through `preset` changed parameter values");
            check(choice->getIndex() == 32, "`preset` reads back the selection");

            // ...and the user half of the list loads the file.
            const int userIndex = fresh.factoryPresetCount();
            fresh.applyPresetChoice(userIndex);
            std::printf("applyPresetChoice(%d) [\"%s\"] -> cutoff %.6f (captured %.6f)\n",
                        userIndex, choice->choices[userIndex].toRawUTF8(),
                        fCutoff->getValue(), captured.at(param::cutoff));
            check(fCutoff->getValue() == captured.at(param::cutoff),
                  "selecting the USER entry restores the saved value exactly");
        }

        // --- 4. regressions ------------------------------------------------
        // A MIDI program change is applied by the 30 Hz timer, so this half
        // has to come back through the message loop rather than block it.
        regression = std::make_unique<SappSynthProcessor>();
        juce::AudioBuffer<float> buffer(2, 64);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::programChange(1, 21), 0);   // "Metal Drone"
        regression->prepareToPlay(48000.0, 64);
        regression->processBlock(buffer, midi);

        juce::Timer::callAfterDelay(200, [this, check, normalisedState]
        {
            std::printf("after MIDI program change 21: getCurrentProgram()=%d (\"%s\")\n",
                        regression->getCurrentProgram(),
                        regression->getProgramName(regression->getCurrentProgram()).toRawUTF8());
            check(regression->getCurrentProgram() == 21,
                  "MIDI program change still selects factory presets");
            auto* choiceNow = dynamic_cast<juce::AudioParameterChoice*>(
                regression->apvts.getParameter(sapp::userpresets::kPresetParamId));
            check(choiceNow != nullptr && choiceNow->getIndex() == 21,
                  "`preset` followed the MIDI program change");

            // Host state round-trip. Untouched (default) state is exact; an
            // edited value loses ~1e-8 in the APVTS plain-value -> XML text ->
            // double -> float path. That is pre-existing serialisation
            // behaviour on a path the preset work does not touch, which is why
            // presets store NORMALISED values instead (sapplink/PRESETS.md).
            auto stateRoundTripDiff = [&](sappsynth::SappSynthProcessor& p) {
                juce::MemoryBlock blob;
                p.getStateInformation(blob);
                sappsynth::SappSynthProcessor reloaded;
                reloaded.setStateInformation(blob.getData(), (int) blob.getSize());
                const auto a = normalisedState(p), b = normalisedState(reloaded);
                float worstDiff = 0.0f;
                juce::String worstId;
                for (const auto& [id, value] : a)
                    if (std::abs(b.at(id) - value) > worstDiff)
                        { worstDiff = std::abs(b.at(id) - value); worstId = id; }
                std::printf("host state blob %d bytes, max parameter difference %.9g (%s)\n",
                            (int) blob.getSize(), worstDiff, worstId.toRawUTF8());
                return worstDiff;
            };
            sappsynth::SappSynthProcessor untouched;
            check(stateRoundTripDiff(untouched) == 0.0f,
                  "host state round-trips EXACTLY for an untouched instance");
            regression->apvts.getParameter(sappsynth::param::cutoff)->setValueNotifyingHost(0.4242f);
            check(stateRoundTripDiff(*regression) < 1.0e-6f,
                  "host state round-trips for an edited instance (pre-existing float text precision)");

            std::printf("\n%s (%d failure(s))\n",
                        presetFailures == 0 ? "PRESET TEST PASSED" : "PRESET TEST FAILED",
                        presetFailures);
            setApplicationReturnValue(presetFailures == 0 ? 0 : 1);
            regression.reset();
            quit();
        });
    }

    // --loadpreset "<name>": load a user preset by name into a fresh
    // processor and print what the parameters became. Proves a preset written
    // by something OTHER than this plugin (Claude via the MCP save_preset
    // tool, say) actually lands on the parameters.
    void runLoadPreset(const juce::String& name)
    {
        sappsynth::SappSynthProcessor proc;
        juce::String error;
        const bool ok = proc.loadUserPreset(name, error);
        std::printf("load \"%s\": %s%s\n", name.toRawUTF8(), ok ? "OK" : "FAILED ",
                    ok ? "" : error.toRawUTF8());
        if (ok)
            for (auto* parameter : proc.getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                    if (ranged->paramID == sappsynth::param::cutoff
                        || ranged->paramID == sappsynth::param::resonance
                        || ranged->paramID == sappsynth::param::ampRelease)
                        std::printf("  %-22s normalised %.9g  (plain %.4f)\n",
                                    ranged->paramID.toRawUTF8(), ranged->getValue(),
                                    ranged->convertFrom0to1(ranged->getValue()));
        setApplicationReturnValue(ok ? 0 : 1);
        quit();
    }

    void initialise(const juce::String& commandLine) override
    {
        if (commandLine.startsWith("--loadpreset"))
        {
            runLoadPreset(commandLine.fromFirstOccurrenceOf(" ", false, false).trim().unquoted());
            return;
        }
        if (commandLine.contains("--cctest"))
        {
            runCcTest();
            return;
        }
        if (commandLine.contains("--presettest"))
        {
            runPresetTest();
            return;
        }

        const juce::String outPath = commandLine.trim().isNotEmpty()
            ? commandLine.trim().unquoted() : juce::String("/tmp/sappsynth-ui.png");

        processor = std::make_unique<sappsynth::SappSynthProcessor>();
        processor->prepareToPlay(48000.0, 512);
        editor.reset(processor->createEditor());

        // Let the message loop settle (fonts, images), then snapshot.
        juce::Timer::callAfterDelay(500, [this, outPath]
        {
            auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
            juce::File file(outPath);
            file.deleteFile();
            juce::FileOutputStream stream(file);
            juce::PNGImageFormat png;
            if (stream.openedOk() && png.writeImageToStream(snapshot, stream))
                std::printf("wrote %s (%dx%d)\n", outPath.toRawUTF8(),
                            snapshot.getWidth(), snapshot.getHeight());
            else
                std::printf("FAILED to write %s\n", outPath.toRawUTF8());
            editor.reset();
            processor.reset();
            quit();
        });
    }

    void shutdown() override {}

private:
    std::unique_ptr<sappsynth::SappSynthProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::unique_ptr<sappsynth::SappSynthProcessor> regression;   // --presettest
    int presetFailures = 0;
};

START_JUCE_APPLICATION(UiShotApp)
