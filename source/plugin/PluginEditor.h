#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

namespace sappsynth {

// Dark, flat, section-based panel. Custom rotary drawing, no image assets.
class SappLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SappLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
};

class SappSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit SappSynthEditor(SappSynthProcessor&);
    ~SappSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Chooser
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::vector<juce::Component*> slots; // laid out left-to-right
    };

    Knob& addKnob(const char* paramId, const juce::String& text);
    Chooser& addChooser(const char* paramId, const juce::String& text);
    Section& addSection(const juce::String& title);

    void applyPreset(int index);
    void refreshSeedLabel();

    SappSynthProcessor& processor;
    SappLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Chooser>> choosers;
    std::vector<Section> sections;

    juce::ComboBox presetBox;
    juce::TextButton newUnitButton { "NEW UNIT" };
    juce::Label seedLabel;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappSynthEditor)
};

} // namespace sappsynth
