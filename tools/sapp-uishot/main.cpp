// sapp-uishot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappUiShot [output.png]

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SappUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise(const juce::String& commandLine) override
    {
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
