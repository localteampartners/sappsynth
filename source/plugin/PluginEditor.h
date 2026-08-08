#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "FactoryPresets.h"

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
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

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
    std::vector<float> sagHistory, driftHistory; // UI-side rolling timeline
    int lastActiveVoice { -1 };
};

// Searchable, scrollable preset browser overlay: type to filter across name
// and category, click to load. Lists the factory bank first, then the user
// presets found in <Documents>/SappSounds/presets/sappsynth (category "USER").
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    PresetBrowser();

    // index < factory count -> presets::all()[index]; beyond that it is a
    // user preset and userNameAt(index) is the name to load.
    std::function<void(int)> onPresetChosen;

    // Re-read the user preset folder. Cheap; called every time the browser
    // opens so a preset saved this session shows up straight away.
    void refreshUserPresets();
    int factoryCount() const;
    juce::String userNameAt(int index) const;

    void resized() override;
    void paint(juce::Graphics&) override;
    void visibilityChanged() override;

private:
    int getNumRows() override { return static_cast<int>(filtered.size()); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void refilter();

    struct Entry { juce::String name, category; };
    std::vector<Entry> entries;   // factory bank, then user presets
    juce::TextEditor searchBox;
    juce::ListBox list { "presets", this };
    std::vector<int> filtered;    // indices into entries
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
    void promptSaveUserPreset();
    void refreshSeedLabel();
    void layoutRow(const std::vector<int>& sectionIndices, juce::Rectangle<int> rowArea);

    SappSynthProcessor& processor;
    VintageLookAndFeel lookAndFeel;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Chooser>> choosers;
    std::vector<Section> sections;

    juce::TextButton presetButton { "PRESETS" };
    juce::TextButton savePresetButton { "SAVE" };
    PresetBrowser presetBrowser;
    std::unique_ptr<juce::AlertWindow> saveWindow;
    juce::TextButton newUnitButton { "NEW UNIT" };
    juce::TextButton lockButton { "LOCK" };
    juce::Label seedLabel;
    LabPanel labPanel;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappSynthEditor)
};

} // namespace sappsynth
