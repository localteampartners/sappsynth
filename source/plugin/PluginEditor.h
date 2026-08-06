#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

namespace sappsynth {

// Vintage hardware look: generated photoreal assets (walnut cheeks, crinkle
// panel, bakelite filmstrip knobs) — see scripts/generate_ui_assets.py.
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    juce::Image woodImage, panelImage, screwImage;
    juce::Image knobCream, knobBlack;
    static constexpr int kFrames = 101;
    static constexpr int kFrameSize = 96;
};

// Minimal Lab view (architecture §19): phosphor-scope + spectrum fed by the
// engine's lock-free telemetry tap, ideal-vs-modeled A/B, drift freeze.
class LabPanel : public juce::Component, private juce::Timer
{
public:
    explicit LabPanel(SappSynthProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SappSynthProcessor& processor;
    juce::TextButton idealButton { "IDEAL" };
    juce::TextButton freezeButton { "FREEZE DRIFT" };
    std::vector<float> capture;
    std::vector<float> spectrum;
    juce::dsp::FFT fft { 11 }; // 2048
    std::vector<float> fftData;
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
        std::vector<juce::Component*> slots;
    };

    Knob& addKnob(const char* paramId, const juce::String& text, bool small = false);
    Chooser& addChooser(const char* paramId, const juce::String& text);
    Section& addSection(const juce::String& title);

    void applyPreset(int index);
    void refreshSeedLabel();
    void layoutRow(const std::vector<int>& sectionIndices, juce::Rectangle<int> rowArea);

    SappSynthProcessor& processor;
    VintageLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Chooser>> choosers;
    std::vector<Section> sections;

    juce::ComboBox presetBox;
    juce::TextButton newUnitButton { "NEW UNIT" };
    juce::Label seedLabel;
    LabPanel labPanel;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappSynthEditor)
};

} // namespace sappsynth
