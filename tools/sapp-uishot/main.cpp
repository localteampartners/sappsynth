// sapp-uishot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappUiShot [output.png]

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
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

    void initialise(const juce::String& commandLine) override
    {
        if (commandLine.contains("--cctest"))
        {
            runCcTest();
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
};

START_JUCE_APPLICATION(UiShotApp)
