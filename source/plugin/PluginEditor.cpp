#include "PluginEditor.h"
#include "../parameters/ParameterIds.h"

namespace sappsynth {

namespace colours {
const juce::Colour background { 0xff14161a };
const juce::Colour panel      { 0xff1c1f26 };
const juce::Colour panelLine  { 0xff2a2e37 };
const juce::Colour accent     { 0xfff5a623 };
const juce::Colour accentDim  { 0xff8a6a2f };
const juce::Colour text       { 0xffc9ced8 };
const juce::Colour textDim    { 0xff7d8494 };
} // namespace colours

// ------------------------------------------------------------- look & feel --
SappLookAndFeel::SappLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, colours::textDim);
    setColour(juce::ComboBox::backgroundColourId, colours::panel.brighter(0.06f));
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::outlineColourId, colours::panelLine);
    setColour(juce::ComboBox::arrowColourId, colours::accent);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::accentDim);
    setColour(juce::TextButton::buttonColourId, colours::panel.brighter(0.08f));
    setColour(juce::TextButton::textColourOffId, colours::accent);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffd8dbe2));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff23262d));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha(0.8f));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha(0.25f));
    setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
}

void SappLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcThickness = juce::jmax(2.4f, radius * 0.16f);
    const float arcRadius = radius - arcThickness * 0.5f;

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(colours::panelLine);
    g.strokePath(track, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour(colours::accent);
    g.strokePath(value, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    const float knobRadius = arcRadius - arcThickness * 1.1f;
    g.setColour(colours::panel.brighter(0.10f));
    g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour(colours::panelLine.brighter(0.15f));
    g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    const auto tip = centre.getPointOnCircumference(knobRadius * 0.75f, angle);
    g.setColour(colours::accent);
    g.drawLine(centre.x + (tip.x - centre.x) * 0.35f, centre.y + (tip.y - centre.y) * 0.35f,
               tip.x, tip.y, 2.2f);
}

// ------------------------------------------------------------------ editor --
namespace {

struct PresetValue { const char* id; float value; };
struct Preset { const char* name; std::vector<PresetValue> values; };

// Factory presets: values in real parameter units, everything else at default.
const std::vector<Preset>& factoryPresets()
{
    namespace p = param;
    static const std::vector<Preset> presets {
        { "Init", {} },
        { "Warm Brass", {
            { p::osc2Level, 0.85f }, { p::osc2Fine, 4.0f }, { p::cutoff, 750.0f },
            { p::resonance, 0.15f }, { p::filterEnvAmt, 0.45f }, { p::filtDecay, 0.35f },
            { p::filtSustain, 0.4f }, { p::ampAttack, 0.04f }, { p::ampRelease, 0.3f },
            { p::mixerDrive, 2.2f }, { p::character, 0.7f }, { p::driftAmount, 2.0f } } },
        { "Acid Bass", {
            { p::cutoff, 240.0f }, { p::resonance, 0.88f }, { p::filterEnvAmt, 0.75f },
            { p::filtDecay, 0.18f }, { p::filtSustain, 0.0f }, { p::keyTrack, 0.8f },
            { p::filterDrive, 10.0f }, { p::subLevel, 0.4f }, { p::ampDecay, 0.3f },
            { p::ampRelease, 0.12f }, { p::polyphony, 1.0f }, { p::filterVel, 0.6f } } },
        { "PWM Strings", {
            { p::osc1Wave, 2.0f }, { p::osc1Pw, 0.32f }, { p::osc2Wave, 2.0f },
            { p::osc2Pw, 0.68f }, { p::osc2Fine, -6.0f }, { p::osc2Level, 0.8f },
            { p::cutoff, 3800.0f }, { p::ampAttack, 0.25f }, { p::ampRelease, 0.8f },
            { p::lfoRate, 0.8f }, { p::lfoToPitch, 3.0f }, { p::character, 0.8f },
            { p::driftAmount, 2.5f } } },
        { "Drift Pad", {
            { p::osc2Level, 0.9f }, { p::osc2Fine, 9.0f }, { p::subLevel, 0.5f },
            { p::cutoff, 1600.0f }, { p::ampAttack, 0.6f }, { p::ampRelease, 1.2f },
            { p::filtAttack, 0.8f }, { p::filterEnvAmt, 0.3f }, { p::character, 1.0f },
            { p::driftAmount, 4.0f }, { p::driftSpeed, 0.3f }, { p::warmup, 0.4f },
            { p::quality, 2.0f } } },
        { "Punchy Pluck", {
            { p::osc2Wave, 2.0f }, { p::osc2Semi, -12.0f }, { p::osc2Level, 0.6f },
            { p::cutoff, 900.0f }, { p::resonance, 0.35f }, { p::filterEnvAmt, 0.8f },
            { p::filtDecay, 0.12f }, { p::filtSustain, 0.0f }, { p::ampDecay, 0.35f },
            { p::ampSustain, 0.0f }, { p::ampRelease, 0.25f }, { p::keyTrack, 1.0f },
            { p::filterVel, 0.6f }, { p::character, 0.6f } } },
        { "Whistle (Self-Osc)", {
            { p::osc1Level, 0.0f }, { p::noiseLevel, 0.06f }, { p::cutoff, 880.0f },
            { p::resonance, 1.0f }, { p::keyTrack, 1.0f }, { p::filterEnvAmt, 0.0f },
            { p::ampAttack, 0.02f }, { p::ampRelease, 0.5f }, { p::quality, 2.0f } } },
    };
    return presets;
}

} // namespace

SappSynthEditor::SappSynthEditor(SappSynthProcessor& proc)
    : juce::AudioProcessorEditor(proc),
      processor(proc),
      keyboard(proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);
    namespace p = param;
    sections.reserve(16); // addSection returns references — no reallocation

    // Row 1
    auto& osc1 = addSection("OSC 1");
    {
        auto& wave = addChooser(p::osc1Wave, "Wave");
        osc1.slots = { &wave.box, &addKnob(p::osc1Octave, "Octave").slider,
                       &addKnob(p::osc1Semi, "Semi").slider, &addKnob(p::osc1Fine, "Fine").slider,
                       &addKnob(p::osc1Pw, "PW").slider, &addKnob(p::osc1Level, "Level").slider };
    }
    auto& osc2 = addSection("OSC 2");
    {
        auto& wave = addChooser(p::osc2Wave, "Wave");
        osc2.slots = { &wave.box, &addKnob(p::osc2Octave, "Octave").slider,
                       &addKnob(p::osc2Semi, "Semi").slider, &addKnob(p::osc2Fine, "Fine").slider,
                       &addKnob(p::osc2Pw, "PW").slider, &addKnob(p::osc2Level, "Level").slider };
    }
    auto& subSection = addSection("SUB / NOISE");
    subSection.slots = { &addChooser(p::subOctave, "Sub Oct").box, &addChooser(p::subWave, "Sub Wave").box,
                         &addKnob(p::subLevel, "Sub").slider, &addKnob(p::noiseLevel, "Noise").slider };
    auto& mixerSection = addSection("MIXER");
    mixerSection.slots = { &addKnob(p::mixerDrive, "Drive").slider,
                           &addKnob(p::mixerChar, "Character").slider };

    // Row 2
    auto& filterSection = addSection("LADDER FILTER");
    filterSection.slots = { &addKnob(p::cutoff, "Cutoff").slider, &addKnob(p::resonance, "Res").slider,
                            &addKnob(p::filterDrive, "Drive").slider, &addKnob(p::keyTrack, "Key Trk").slider,
                            &addKnob(p::filterEnvAmt, "Env Amt").slider, &addKnob(p::filterVel, "Vel").slider };
    auto& ampEnvSection = addSection("AMP ENV");
    ampEnvSection.slots = { &addKnob(p::ampAttack, "A").slider, &addKnob(p::ampDecay, "D").slider,
                            &addKnob(p::ampSustain, "S").slider, &addKnob(p::ampRelease, "R").slider };
    auto& filtEnvSection = addSection("FILTER ENV");
    filtEnvSection.slots = { &addKnob(p::filtAttack, "A").slider, &addKnob(p::filtDecay, "D").slider,
                             &addKnob(p::filtSustain, "S").slider, &addKnob(p::filtRelease, "R").slider };
    auto& lfoSection = addSection("LFO");
    lfoSection.slots = { &addChooser(p::lfoShape, "Shape").box, &addKnob(p::lfoRate, "Rate").slider,
                         &addKnob(p::lfoToPitch, "→ Pitch").slider, &addKnob(p::lfoToCutoff, "→ Cutoff").slider };

    // Row 3
    auto& analogSection = addSection("ANALOG CHARACTER");
    analogSection.slots = { &addKnob(p::character, "Amount").slider, &addKnob(p::driftAmount, "Drift").slider,
                            &addKnob(p::driftSpeed, "Speed").slider, &addKnob(p::warmup, "Warm-up").slider };
    auto& voiceSection = addSection("VOICE");
    voiceSection.slots = { &addKnob(p::polyphony, "Poly").slider };
    auto& outSection = addSection("OUTPUT");
    outSection.slots = { &addKnob(p::outputDrive, "Drive").slider, &addKnob(p::master, "Master").slider,
                         &addChooser(p::quality, "Quality").box };

    // Header widgets
    presetBox.setTextWhenNothingSelected("Presets");
    int id = 1;
    for (const auto& preset : factoryPresets())
        presetBox.addItem(preset.name, id++);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0)
            applyPreset(index);
    };
    addAndMakeVisible(presetBox);

    newUnitButton.onClick = [this]
    {
        processor.rerollUnitSeed();
        refreshSeedLabel();
    };
    addAndMakeVisible(newUnitButton);

    seedLabel.setJustificationType(juce::Justification::centredLeft);
    seedLabel.setColour(juce::Label::textColourId, colours::textDim);
    refreshSeedLabel();
    addAndMakeVisible(seedLabel);

    keyboard.setKeyWidth(22.0f);
    keyboard.setAvailableRange(24, 108);
    addAndMakeVisible(keyboard);

    setSize(1150, 596);
}

SappSynthEditor::~SappSynthEditor()
{
    setLookAndFeel(nullptr);
}

SappSynthEditor::Knob& SappSynthEditor::addKnob(const char* paramId, const juce::String& text)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 14);
    knob->slider.setColour(juce::Slider::textBoxTextColourId, colours::textDim);
    knob->label.setText(text, juce::dontSendNotification);
    knob->label.setJustificationType(juce::Justification::centred);
    knob->label.setFont(juce::Font(juce::FontOptions(11.0f)));
    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, paramId, knob->slider);
    addAndMakeVisible(knob->slider);
    addAndMakeVisible(knob->label);
    knobs.push_back(std::move(knob));
    return *knobs.back();
}

SappSynthEditor::Chooser& SappSynthEditor::addChooser(const char* paramId, const juce::String& text)
{
    auto chooser = std::make_unique<Chooser>();
    chooser->label.setText(text, juce::dontSendNotification);
    chooser->label.setJustificationType(juce::Justification::centred);
    chooser->label.setFont(juce::Font(juce::FontOptions(11.0f)));
    chooser->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, paramId, chooser->box);
    addAndMakeVisible(chooser->box);
    addAndMakeVisible(chooser->label);
    choosers.push_back(std::move(chooser));
    return *choosers.back();
}

SappSynthEditor::Section& SappSynthEditor::addSection(const juce::String& title)
{
    sections.push_back({ title, {}, {} });
    return sections.back();
}

void SappSynthEditor::applyPreset(int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

    // Reset everything to defaults, then overlay the preset's values.
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            withId->setValueNotifyingHost(withId->getDefaultValue());

    for (const auto& [id, value] : presets[static_cast<std::size_t>(index)].values)
        if (auto* parameter = processor.apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void SappSynthEditor::refreshSeedLabel()
{
    seedLabel.setText("UNIT " + processor.unitSeedText(), juce::dontSendNotification);
}

void SappSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::background);

    // Header
    g.setColour(colours::accent);
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    g.drawText("SAPPSYNTH", 18, 8, 220, 30, juce::Justification::centredLeft);
    g.setColour(colours::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("VIRTUAL ANALOG LABORATORY", 18, 30, 300, 14, juce::Justification::centredLeft);

    // Section panels
    for (const auto& section : sections)
    {
        g.setColour(colours::panel);
        g.fillRoundedRectangle(section.bounds.toFloat(), 6.0f);
        g.setColour(colours::panelLine);
        g.drawRoundedRectangle(section.bounds.toFloat(), 6.0f, 1.0f);
        g.setColour(colours::textDim);
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        g.drawText(section.title, section.bounds.getX() + 10, section.bounds.getY() + 5,
                   section.bounds.getWidth() - 20, 12, juce::Justification::centredLeft);
    }
}

void SappSynthEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // Header strip
    auto header = area.removeFromTop(40);
    header.removeFromLeft(300);
    seedLabel.setBounds(header.removeFromRight(150));
    newUnitButton.setBounds(header.removeFromRight(90).reduced(0, 7));
    header.removeFromRight(10);
    presetBox.setBounds(header.removeFromRight(190).reduced(0, 7));

    keyboard.setBounds(area.removeFromBottom(78));
    area.removeFromBottom(8);

    // Three rows of sections; width shared by slot count.
    const int rowGap = 8;
    const int rowHeight = (area.getHeight() - 2 * rowGap) / 3;
    const std::vector<std::vector<int>> rows { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10 } };

    auto layoutSlot = [&](juce::Component* component, juce::Rectangle<int> cell)
    {
        // Combo boxes sit vertically centred; sliders fill the cell. The
        // paired label is the next sibling in our owned vectors.
        if (auto* box = dynamic_cast<juce::ComboBox*>(component))
        {
            box->setBounds(cell.removeFromTop(cell.getHeight() - 16).withSizeKeepingCentre(cell.getWidth() - 6, 24));
            for (const auto& chooser : choosers)
                if (&chooser->box == box)
                    chooser->label.setBounds(cell);
        }
        else if (auto* slider = dynamic_cast<juce::Slider*>(component))
        {
            slider->setBounds(cell.removeFromTop(cell.getHeight() - 16));
            for (const auto& knob : knobs)
                if (&knob->slider == slider)
                    knob->label.setBounds(cell);
        }
    };

    int rowY = area.getY();
    for (const auto& row : rows)
    {
        int totalSlots = 0;
        for (const int sectionIndex : row)
            totalSlots += static_cast<int>(sections[static_cast<std::size_t>(sectionIndex)].slots.size());

        const int sectionGap = 8;
        const int gaps = (static_cast<int>(row.size()) - 1) * sectionGap;
        const float slotWidth = static_cast<float>(area.getWidth() - gaps) / static_cast<float>(totalSlots);

        int x = area.getX();
        for (const int sectionIndex : row)
        {
            auto& section = sections[static_cast<std::size_t>(sectionIndex)];
            const int width = static_cast<int>(slotWidth * static_cast<float>(section.slots.size()));
            section.bounds = { x, rowY, width, rowHeight };

            auto inner = section.bounds.reduced(6).withTrimmedTop(16);
            const int cellWidth = inner.getWidth() / juce::jmax(1, static_cast<int>(section.slots.size()));
            for (auto* slot : section.slots)
                layoutSlot(slot, inner.removeFromLeft(cellWidth));

            x += width + sectionGap;
        }
        rowY += rowHeight + rowGap;
    }
}

} // namespace sappsynth
