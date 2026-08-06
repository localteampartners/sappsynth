#include "PluginEditor.h"
#include "../parameters/ParameterIds.h"
#include "BinaryData.h"

namespace sappsynth {

namespace colours {
const juce::Colour cream      { 0xffe8e0ce };
const juce::Colour creamDim   { 0xffb9b2a0 };
const juce::Colour amber      { 0xffd99a2b };
const juce::Colour panelDark  { 0xff1c1d20 };
const juce::Colour engrave    { 0xff35363b };
const juce::Colour engraveHi  { 0xff56575e };
const juce::Colour phosphor   { 0xff46e06a };
const juce::Colour phosphorDim{ 0xff1e3a24 };
const juce::Colour crt        { 0xff0a120c };
} // namespace colours

static juce::Font vintageFont(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions("Futura", size, bold ? juce::Font::bold : juce::Font::plain));
}

// ------------------------------------------------------------- look & feel --
VintageLookAndFeel::VintageLookAndFeel()
{
    auto load = [](const char* name)
    {
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(name, dataSize);
        return juce::ImageCache::getFromMemory(data, dataSize);
    };
    woodImage = load("wood_side_png");
    panelImage = load("panel_png");
    screwImage = load("screw_png");
    knobCream = load("knob_cream_png");
    knobBlack = load("knob_black_png");

    setColour(juce::Label::textColourId, colours::creamDim);
    setColour(juce::ComboBox::textColourId, colours::cream);
    setColour(juce::ComboBox::arrowColourId, colours::amber);
    setColour(juce::PopupMenu::backgroundColourId, colours::panelDark);
    setColour(juce::PopupMenu::textColourId, colours::cream);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff5c4718));
    setColour(juce::TextButton::textColourOffId, colours::creamDim);
    setColour(juce::TextButton::textColourOnId, colours::amber);
    setColour(juce::Slider::textBoxTextColourId, colours::cream);
    setColour(juce::BubbleComponent::backgroundColourId, colours::panelDark);
    setColour(juce::BubbleComponent::outlineColourId, colours::engraveHi);
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe6e1d4));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff191a1c));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::amber.withAlpha(0.75f));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::amber.withAlpha(0.22f));
    setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha(0.4f));
}

void VintageLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float pos, float, float, juce::Slider& slider)
{
    const bool small = static_cast<bool>(slider.getProperties()["small"]);
    const auto& strip = small ? knobBlack : knobCream;
    if (!strip.isValid())
        return;

    const int cell = juce::jmin(width, height, small ? 66 : 86);
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

    // Graduated 0-10 panel scale: the knob reads at a glance from its pointer.
    const float knobSize = static_cast<float>(cell) * 0.72f;
    const float tickInner = knobSize * 0.54f;
    const float tickOuter = knobSize * 0.62f;
    const float numeralRadius = knobSize * 0.76f;
    const float startAngle = juce::degreesToRadians(-135.0f);
    const float endAngle = juce::degreesToRadians(135.0f);

    g.setColour(juce::Colour(0xffb9b2a0).withAlpha(0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * static_cast<float>(i) / 10.0f;
        const float sa = std::sin(a), ca = -std::cos(a);
        const bool major = small ? (i % 5 == 0) : (i % 2 == 0);
        const float inner = major ? tickInner : tickInner + (tickOuter - tickInner) * 0.35f;
        g.drawLine(cx + sa * inner, cy + ca * inner, cx + sa * tickOuter, cy + ca * tickOuter,
                   major ? 1.4f : 0.9f);
    }
    g.setFont(juce::Font(juce::FontOptions("Futura", small ? 7.5f : 8.5f, juce::Font::bold)));
    for (int i = 0; i <= 10; i += small ? 10 : 5)
    {
        const float a = startAngle + (endAngle - startAngle) * static_cast<float>(i) / 10.0f;
        const float sa = std::sin(a), ca = -std::cos(a);
        g.drawText(juce::String(i),
                   juce::Rectangle<float>(16.0f, 10.0f)
                       .withCentre({ cx + sa * numeralRadius, cy + ca * numeralRadius })
                       .toNearestInt(),
                   juce::Justification::centred);
    }

    const int frame = juce::jlimit(0, kFrames - 1, juce::roundToInt(pos * (kFrames - 1)));
    const int size = static_cast<int>(knobSize);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(strip, static_cast<int>(cx) - size / 2, static_cast<int>(cy) - size / 2,
                size, size, 0, frame * kFrameSize, kFrameSize, kFrameSize);
}

void VintageLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
    // Recessed metal plate: dark fill, shadow on top edge, light on bottom.
    g.setColour(juce::Colour(0xff121316));
    g.fillRoundedRectangle(r.reduced(1.0f), 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawLine(r.getX() + 3, r.getY() + 1.5f, r.getRight() - 3, r.getY() + 1.5f, 1.0f);
    g.setColour(colours::engraveHi.withAlpha(0.5f));
    g.drawRoundedRectangle(r.reduced(1.0f), 3.0f, 1.0f);

    juce::Path arrow;
    const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
    arrow.addTriangle(ax - 4, ay - 2.5f, ax + 4, ay - 2.5f, ax, ay + 3.5f);
    g.setColour(colours::amber);
    g.fillPath(arrow);
}

void VintageLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = button.getToggleState() || down;

    // Bakelite pushbutton: raised when off, lit amber ring when engaged.
    juce::ColourGradient grad(juce::Colour(on ? 0xff3a3122 : 0xff2b2c30), r.getX(), r.getY(),
                              juce::Colour(on ? 0xff241d10 : 0xff17181b), r.getX(), r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(on ? colours::amber.withAlpha(0.9f)
                   : colours::engraveHi.withAlpha(highlighted ? 0.9f : 0.55f));
    g.drawRoundedRectangle(r, 4.0f, on ? 1.4f : 1.0f);
    if (!on)
    {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawLine(r.getX() + 3, r.getY() + 1.5f, r.getRight() - 3, r.getY() + 1.5f, 1.0f);
    }
}

juce::Font VintageLookAndFeel::getComboBoxFont(juce::ComboBox&) { return vintageFont(11.5f); }

void VintageLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // Narrow panel cells: give the text everything except the arrow.
    label.setBounds(4, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}
juce::Font VintageLookAndFeel::getTextButtonFont(juce::TextButton&, int) { return vintageFont(11.5f, true); }

// --------------------------------------------------------------- lab panel --
LabPanel::LabPanel(SappSynthProcessor& proc) : processor(proc)
{
    capture.resize(4096, 0.0f);
    spectrum.resize(256, -100.0f);
    fftData.resize(2 * 2048, 0.0f);

    // Cycles the three operating modes: Ideal Digital -> Analog DNA ->
    // Exaggerated Demonstration (dna.md). Click-free: applied at control rate.
    idealButton.setTooltip("Cycle: Ideal Digital / Analog DNA / Exaggerated Demonstration");
    idealButton.onClick = [this]
    {
        auto& eng = processor.synthEngine();
        const int next = (static_cast<int>(eng.currentLabMode()) + 1) % 3;
        eng.setLabMode(static_cast<SynthEngine::LabMode>(next));
        idealButton.setButtonText(next == 0 ? "IDEAL" : next == 1 ? "ANALOG DNA" : "EXAGGERATED");
        idealButton.setToggleState(next != 1, juce::dontSendNotification);
    };
    idealButton.setButtonText("ANALOG DNA");
    addAndMakeVisible(idealButton);

    freezeButton.setClickingTogglesState(true);
    freezeButton.onClick = [this]
    {
        processor.synthEngine().setDriftFrozen(freezeButton.getToggleState());
    };
    addAndMakeVisible(freezeButton);

    startTimerHz(30);
}

void LabPanel::timerCallback()
{
    processor.synthEngine().telemetry().readLatest(capture.data(), static_cast<int>(capture.size()));

    // Spectrum: Hann-windowed FFT of the freshest 2048 samples.
    const int n = 2048;
    const float* src = capture.data() + capture.size() - n;
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                 * static_cast<float>(i) / (n - 1)));
        fftData[static_cast<std::size_t>(i)] = src[i] * w;
    }
    std::fill(fftData.begin() + n, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    for (std::size_t b = 0; b < spectrum.size(); ++b)
    {
        // Log frequency mapping across bins 2..1024.
        const float frac = static_cast<float>(b) / static_cast<float>(spectrum.size() - 1);
        const float bin = 2.0f * std::pow(512.0f, frac);
        const float mag = fftData[static_cast<std::size_t>(bin)];
        const float db = juce::Decibels::gainToDecibels(mag / (n * 0.25f), -100.0f);
        // Fast attack, slow release smoothing keeps the display readable.
        spectrum[b] = db > spectrum[b] ? db : spectrum[b] * 0.92f + db * 0.08f;
    }

    // Timeline: UI-side rolling history of shared circuit state (the audio
    // thread only writes relaxed atomics).
    const auto& diag = processor.synthEngine().diagnostics();
    sagHistory.push_back(diag.supplySag.load(std::memory_order_relaxed));
    driftHistory.push_back(diag.unitDriftNorm.load(std::memory_order_relaxed));
    if (sagHistory.size() > 240) { sagHistory.erase(sagHistory.begin()); driftHistory.erase(driftHistory.begin()); }

    const auto& vm = processor.synthEngine().voices();
    for (int i = 0; i < 16; ++i)
        if (vm.voice(i).isActive())
            lastActiveVoice = i;

    repaint();
}

void LabPanel::resized()
{
    auto r = getLocalBounds().reduced(6);
    auto buttonRow = r.removeFromBottom(22);
    idealButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(2, 0));
    freezeButton.setBounds(buttonRow.reduced(2, 0));
}

void LabPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().reduced(6);
    r.removeFromBottom(26);
    auto fingerprintArea = r.removeFromBottom(18);
    auto scopeArea = r.removeFromTop(r.getHeight() / 2).toFloat();
    auto specArea = r.toFloat();

    for (const auto& area : { scopeArea, specArea })
    {
        g.setColour(colours::crt);
        g.fillRoundedRectangle(area, 3.0f);
        g.setColour(colours::phosphorDim);
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine(static_cast<int>(area.getY() + area.getHeight() * i / 4.0f),
                                 area.getX() + 2, area.getRight() - 2);
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine(static_cast<int>(area.getX() + area.getWidth() * i / 8.0f),
                               area.getY() + 2, area.getBottom() - 2);
        g.setColour(colours::engrave);
        g.drawRoundedRectangle(area, 3.0f, 1.0f);
    }

    // --- scope: trigger on a rising zero crossing in the middle of capture ---
    const int window = 700;
    int trigger = static_cast<int>(capture.size()) - window - 1;
    for (int i = static_cast<int>(capture.size()) / 2;
         i < static_cast<int>(capture.size()) - window - 1; ++i)
        if (capture[static_cast<std::size_t>(i)] <= 0.0f && capture[static_cast<std::size_t>(i + 1)] > 0.0f)
        {
            trigger = i;
            break;
        }

    juce::Path trace;
    for (int i = 0; i < window; ++i)
    {
        const float v = juce::jlimit(-1.2f, 1.2f, capture[static_cast<std::size_t>(trigger + i)]);
        const float px = scopeArea.getX() + scopeArea.getWidth() * static_cast<float>(i) / (window - 1);
        const float py = scopeArea.getCentreY() - v * scopeArea.getHeight() * 0.42f;
        if (i == 0)
            trace.startNewSubPath(px, py);
        else
            trace.lineTo(px, py);
    }
    g.setColour(colours::phosphor.withAlpha(0.28f));
    g.strokePath(trace, juce::PathStrokeType(2.8f));
    g.setColour(colours::phosphor);
    g.strokePath(trace, juce::PathStrokeType(1.1f));

    // --- spectrum ---
    juce::Path spec;
    for (std::size_t b = 0; b < spectrum.size(); ++b)
    {
        const float frac = static_cast<float>(b) / static_cast<float>(spectrum.size() - 1);
        const float norm = juce::jlimit(0.0f, 1.0f, (spectrum[b] + 90.0f) / 90.0f);
        const float px = specArea.getX() + specArea.getWidth() * frac;
        const float py = specArea.getBottom() - 2.0f - norm * (specArea.getHeight() - 6.0f);
        if (b == 0)
            spec.startNewSubPath(px, py);
        else
            spec.lineTo(px, py);
    }
    g.setColour(colours::phosphor.withAlpha(0.25f));
    g.strokePath(spec, juce::PathStrokeType(2.6f));
    g.setColour(colours::phosphor.withAlpha(0.9f));
    g.strokePath(spec, juce::PathStrokeType(1.0f));

    g.setColour(colours::phosphor.withAlpha(0.5f));
    g.setFont(vintageFont(9.0f, true));
    g.drawText("SCOPE", scopeArea.toNearestInt().reduced(5, 3), juce::Justification::topRight);
    g.drawText("SPECTRUM", specArea.toNearestInt().reduced(5, 3), juce::Justification::topRight);

    // --- voice fingerprint cells + shared-state timeline ------------------
    const auto& vm = processor.synthEngine().voices();
    auto cells = fingerprintArea.removeFromLeft(fingerprintArea.getWidth() / 2);
    const int cellW = juce::jmax(1, cells.getWidth() / 16);
    g.setFont(vintageFont(7.5f, true));
    for (int i = 0; i < 16; ++i)
    {
        auto cell = juce::Rectangle<int>(cells.getX() + i * cellW, cells.getY() + 2, cellW - 1, 12);
        const bool active = vm.voice(i).isActive();
        g.setColour(active ? colours::amber.withAlpha(0.85f) : colours::panelDark.brighter(0.12f));
        g.fillRect(cell);
        g.setColour(active ? juce::Colours::black : colours::creamDim.withAlpha(0.5f));
        g.drawText(juce::String(i + 1), cell, juce::Justification::centred);
    }
    // Supply sag (amber) + unit drift (green) timeline behind the readout.
    const auto spark = fingerprintArea.reduced(2);
    auto drawSpark = [&](const std::vector<float>& h, juce::Colour c, float scale, float mid)
    {
        if (h.size() < 2) return;
        juce::Path path;
        for (std::size_t i = 0; i < h.size(); ++i)
        {
            const float px = spark.getX() + spark.getWidth() * static_cast<float>(i) / static_cast<float>(h.size() - 1);
            const float py = juce::jlimit(static_cast<float>(spark.getY()), static_cast<float>(spark.getBottom()),
                                          spark.getCentreY() - (h[i] - mid) * scale * spark.getHeight());
            i == 0 ? path.startNewSubPath(px, py) : path.lineTo(px, py);
        }
        g.setColour(c.withAlpha(0.55f));
        g.strokePath(path, juce::PathStrokeType(1.0f));
    };
    drawSpark(sagHistory, colours::amber, 1.2f, 0.3f);
    drawSpark(driftHistory, colours::phosphor, 0.25f, 0.0f);

    if (lastActiveVoice >= 0)
    {
        const auto& prof = vm.voice(lastActiveVoice).profile();
        g.setColour(colours::creamDim);
        g.setFont(vintageFont(8.0f));
        g.drawText(juce::String::formatted("V%d  %+.1fc  filt%+.0fc  atk x%.2f  vca%+.1fdB",
                                           lastActiveVoice + 1, prof.osc1TuneCents,
                                           prof.filterCutoffCents, prof.envAttackScale, prof.vcaGainDb),
                   fingerprintArea.reduced(4, 0), juce::Justification::centredLeft);
    }
}

// ------------------------------------------------------------------ editor --
namespace {

struct PresetValue { const char* id; float value; };
struct Preset { const char* name; const char* category; std::vector<PresetValue> values; };

const std::vector<Preset>& factoryPresets()
{
    namespace p = param;
    static const std::vector<Preset> presets {
        { "Init", "INIT", {} },

        // ---- BASS ----
        { "Acid Bass", "BASS", {
            { p::cutoff, 240.0f }, { p::resonance, 0.88f }, { p::filterEnvAmt, 0.75f },
            { p::filtDecay, 0.18f }, { p::filtSustain, 0.0f }, { p::keyTrack, 0.8f },
            { p::filterDrive, 10.0f }, { p::subLevel, 0.4f }, { p::ampDecay, 0.3f },
            { p::ampRelease, 0.12f }, { p::polyphony, 1.0f }, { p::filterVel, 0.6f },
            { p::glide, 0.06f }, { p::delayMix, 0.22f }, { p::delayTime, 0.19f },
            { p::delayFeedback, 0.45f } } },
        { "Model Growl", "BASS", {
            { p::osc2Level, 0.9f }, { p::osc2Fine, -8.0f }, { p::subLevel, 0.6f },
            { p::cutoff, 350.0f }, { p::resonance, 0.25f }, { p::filterEnvAmt, 0.55f },
            { p::filtDecay, 0.25f }, { p::filtSustain, 0.1f }, { p::keyTrack, 0.4f },
            { p::mixerDrive, 3.5f }, { p::filterDrive, 8.0f }, { p::character, 0.8f },
            { p::polyphony, 1.0f }, { p::glide, 0.04f }, { p::ampRelease, 0.2f } } },
        { "Taurus Rumble", "BASS", {
            { p::osc1Octave, -1.0f }, { p::subOctave, 1.0f }, { p::subLevel, 0.8f },
            { p::cutoff, 200.0f }, { p::mixerDrive, 3.0f }, { p::outputDrive, 6.0f },
            { p::polyphony, 1.0f }, { p::glide, 0.08f }, { p::ampRelease, 0.4f },
            { p::character, 0.9f }, { p::filterEnvAmt, 0.3f }, { p::filtDecay, 0.4f } } },

        // ---- LEAD ----
        { "Fat Fifth Lead", "LEAD", {
            { p::osc2Semi, 7.0f }, { p::osc2Level, 0.9f }, { p::cutoff, 2500.0f },
            { p::resonance, 0.3f }, { p::polyphony, 1.0f }, { p::glide, 0.05f },
            { p::character, 0.7f }, { p::delayMix, 0.25f }, { p::unisonCount, 2.0f },
            { p::unisonDetune, 8.0f }, { p::filterVel, 0.4f } } },
        { "Lucky Lead", "LEAD", {
            { p::osc2Level, 0.9f }, { p::osc2Fine, 6.0f }, { p::unisonCount, 3.0f },
            { p::unisonDetune, 10.0f }, { p::cutoff, 1800.0f }, { p::filterEnvAmt, 0.4f },
            { p::filterVel, 0.5f }, { p::ampDecay, 0.5f }, { p::delayMix, 0.2f },
            { p::character, 0.6f } } },
        { "Hoover Rave", "LEAD", {
            { p::osc2Wave, 2.0f }, { p::osc2Pw, 0.4f }, { p::osc2Level, 0.9f },
            { p::unisonCount, 5.0f }, { p::unisonDetune, 35.0f }, { p::unisonSpread, 1.0f },
            { p::cutoff, 3000.0f }, { p::glide, 0.15f }, { p::chorusMix, 0.4f },
            { p::ampAttack, 0.02f }, { p::character, 0.6f }, { p::polyphony, 10.0f } } },
        { "Whistle (Self-Osc)", "LEAD", {
            { p::osc1Level, 0.0f }, { p::noiseLevel, 0.06f }, { p::cutoff, 880.0f },
            { p::resonance, 1.0f }, { p::keyTrack, 1.0f }, { p::filterEnvAmt, 0.0f },
            { p::ampAttack, 0.02f }, { p::ampRelease, 0.5f }, { p::quality, 2.0f },
            { p::reverbMix, 0.35f } } },

        // ---- KEYS ----
        { "Juno Chorus Keys", "KEYS", {
            { p::subLevel, 0.5f }, { p::cutoff, 4500.0f }, { p::ampAttack, 0.01f },
            { p::ampDecay, 0.4f }, { p::ampSustain, 0.6f }, { p::chorusMix, 0.6f },
            { p::character, 0.6f }, { p::filterVel, 0.4f }, { p::ampRelease, 0.3f } } },
        { "Vintage Organ", "KEYS", {
            { p::osc1Wave, 0.0f }, { p::osc2Wave, 0.0f }, { p::osc2Octave, 1.0f },
            { p::osc2Level, 0.5f }, { p::subWave, 1.0f }, { p::subLevel, 0.8f },
            { p::cutoff, 8000.0f }, { p::ampAttack, 0.005f }, { p::ampSustain, 1.0f },
            { p::ampRelease, 0.06f }, { p::chorusMix, 0.3f }, { p::character, 0.5f } } },
        { "Punchy Pluck", "KEYS", {
            { p::osc2Wave, 2.0f }, { p::osc2Semi, -12.0f }, { p::osc2Level, 0.6f },
            { p::cutoff, 900.0f }, { p::resonance, 0.35f }, { p::filterEnvAmt, 0.8f },
            { p::filtDecay, 0.12f }, { p::filtSustain, 0.0f }, { p::ampDecay, 0.35f },
            { p::ampSustain, 0.0f }, { p::ampRelease, 0.25f }, { p::keyTrack, 1.0f },
            { p::filterVel, 0.6f }, { p::character, 0.6f }, { p::delayMix, 0.18f } } },
        { "Warm Brass", "KEYS", {
            { p::osc2Level, 0.85f }, { p::osc2Fine, 4.0f }, { p::cutoff, 750.0f },
            { p::resonance, 0.15f }, { p::filterEnvAmt, 0.45f }, { p::filtDecay, 0.35f },
            { p::filtSustain, 0.4f }, { p::ampAttack, 0.04f }, { p::ampRelease, 0.3f },
            { p::mixerDrive, 2.2f }, { p::character, 0.7f }, { p::driftAmount, 2.0f },
            { p::chorusMix, 0.2f }, { p::reverbMix, 0.18f } } },

        // ---- FM ----
        { "FM E-Piano", "FM", {
            { p::osc1Wave, 0.0f }, { p::osc2Wave, 0.0f }, { p::osc2Octave, 1.0f },
            { p::osc2Semi, 7.0f }, { p::osc2Level, 0.0f }, { p::osc2Fm, 0.35f },
            { p::cutoff, 8000.0f }, { p::ampDecay, 0.8f }, { p::ampSustain, 0.3f },
            { p::ampRelease, 0.4f }, { p::filterVel, 0.5f }, { p::character, 0.4f },
            { p::reverbMix, 0.15f }, { p::chorusMix, 0.25f } } },
        { "FM Bells", "FM", {
            { p::osc1Wave, 0.0f }, { p::osc2Wave, 0.0f }, { p::osc2Octave, 1.0f },
            { p::osc2Semi, 7.0f }, { p::osc2Fine, 30.0f }, { p::osc2Level, 0.0f },
            { p::osc2Fm, 0.5f }, { p::cutoff, 12000.0f }, { p::ampAttack, 0.005f },
            { p::ampDecay, 1.5f }, { p::ampSustain, 0.0f }, { p::ampRelease, 1.5f },
            { p::reverbMix, 0.4f }, { p::reverbSize, 0.7f }, { p::character, 0.3f } } },
        { "Glass Clang", "FM", {
            { p::osc1Wave, 3.0f }, { p::osc2Wave, 2.0f }, { p::osc2Octave, 1.0f },
            { p::osc2Semi, 6.0f }, { p::osc2Level, 0.2f }, { p::osc2Fm, 0.7f },
            { p::mixerDrive, 2.0f }, { p::cutoff, 9000.0f }, { p::ampDecay, 0.6f },
            { p::ampSustain, 0.0f }, { p::ampRelease, 0.5f }, { p::delayMix, 0.3f } } },
        { "Metal Drone", "FM", {
            { p::osc2Octave, -1.0f }, { p::osc2Fine, 13.0f }, { p::osc2Level, 0.5f },
            { p::osc2Fm, 0.9f }, { p::resonance, 0.5f }, { p::cutoff, 1200.0f },
            { p::character, 1.0f }, { p::driftAmount, 4.0f }, { p::reverbMix, 0.5f },
            { p::ampAttack, 0.5f }, { p::ampRelease, 2.0f }, { p::quality, 2.0f } } },

        // ---- PAD / STRINGS ----
        { "Drift Pad", "PAD", {
            { p::osc2Level, 0.9f }, { p::osc2Fine, 9.0f }, { p::subLevel, 0.5f },
            { p::cutoff, 1600.0f }, { p::ampAttack, 0.6f }, { p::ampRelease, 1.2f },
            { p::filtAttack, 0.8f }, { p::filterEnvAmt, 0.3f }, { p::character, 1.0f },
            { p::driftAmount, 4.0f }, { p::driftSpeed, 0.3f }, { p::warmup, 0.4f },
            { p::quality, 2.0f }, { p::reverbMix, 0.42f }, { p::reverbSize, 0.8f },
            { p::delayMix, 0.18f }, { p::delayTime, 0.55f } } },
        { "Jupiter Sweep Pad", "PAD", {
            { p::osc2Level, 0.9f }, { p::osc2Fine, 5.0f }, { p::osc1Fine, -5.0f },
            { p::cutoff, 900.0f }, { p::lfoToCutoff, 0.35f }, { p::lfoRate, 0.15f },
            { p::ampAttack, 0.8f }, { p::ampRelease, 1.6f }, { p::chorusMix, 0.45f },
            { p::reverbMix, 0.35f }, { p::character, 0.8f }, { p::polyphony, 16.0f },
            { p::unisonCount, 2.0f }, { p::unisonDetune, 9.0f } } },
        { "PWM Strings", "PAD", {
            { p::osc1Wave, 2.0f }, { p::osc1Pw, 0.32f }, { p::osc2Wave, 2.0f },
            { p::osc2Pw, 0.68f }, { p::osc2Fine, -6.0f }, { p::osc2Level, 0.8f },
            { p::cutoff, 3800.0f }, { p::ampAttack, 0.25f }, { p::ampRelease, 0.8f },
            { p::lfoRate, 0.8f }, { p::lfoToPitch, 3.0f }, { p::character, 0.8f },
            { p::driftAmount, 2.5f }, { p::chorusMix, 0.5f }, { p::reverbMix, 0.3f } } },
        { "Polysix Strings", "PAD", {
            { p::osc1Wave, 2.0f }, { p::osc1Pw, 0.35f }, { p::osc2Wave, 2.0f },
            { p::osc2Pw, 0.65f }, { p::osc2Fine, -4.0f }, { p::osc2Level, 0.85f },
            { p::cutoff, 3200.0f }, { p::ampAttack, 0.35f }, { p::ampRelease, 1.0f },
            { p::chorusMix, 0.55f }, { p::reverbMix, 0.25f }, { p::lfoToPitch, 2.0f },
            { p::lfoRate, 0.7f }, { p::character, 0.7f } } },
        { "Super Saw Stack", "PAD", {
            { p::osc2Level, 0.9f }, { p::osc2Fine, -7.0f }, { p::unisonCount, 5.0f },
            { p::unisonDetune, 22.0f }, { p::unisonSpread, 0.9f }, { p::cutoff, 5200.0f },
            { p::ampAttack, 0.02f }, { p::ampRelease, 0.5f }, { p::character, 0.7f },
            { p::chorusMix, 0.25f }, { p::reverbMix, 0.22f }, { p::polyphony, 16.0f } } },
        { "Tape Flute", "PAD", {
            { p::osc1Wave, 0.0f }, { p::osc1Level, 0.9f }, { p::noiseLevel, 0.05f },
            { p::cutoff, 3000.0f }, { p::glide, 0.12f }, { p::character, 1.0f },
            { p::driftAmount, 5.0f }, { p::driftSpeed, 0.6f }, { p::warmup, 0.6f },
            { p::ampAttack, 0.08f }, { p::reverbMix, 0.3f }, { p::polyphony, 1.0f } } },

        // ---- DNA (demonstration presets, dna.md: effects minimal) ----
        { "Ideal Mono Bass", "DNA", {
            { p::character, 0.0f }, { p::cutoff, 500.0f }, { p::subLevel, 0.5f },
            { p::polyphony, 1.0f }, { p::filterEnvAmt, 0.4f }, { p::filtDecay, 0.3f } } },
        { "Analog DNA Mono Bass", "DNA", {
            { p::character, 0.8f }, { p::dnaCondition, 0.5f }, { p::dnaWarmth, 0.6f },
            { p::dnaAge, 0.4f }, { p::cutoff, 500.0f }, { p::subLevel, 0.5f },
            { p::polyphony, 1.0f }, { p::filterEnvAmt, 0.4f }, { p::filtDecay, 0.3f } } },
        { "Warm Oscillator Lead", "DNA", {
            { p::character, 0.9f }, { p::dnaWarmth, 0.9f }, { p::dnaCalibration, 0.6f },
            { p::osc2Level, 0.85f }, { p::osc2Fine, 5.0f }, { p::cutoff, 2200.0f },
            { p::polyphony, 1.0f }, { p::glide, 0.04f } } },
        { "Aging Poly Chords", "DNA", {
            { p::character, 1.0f }, { p::dnaAge, 0.9f }, { p::dnaCondition, 0.8f },
            { p::dnaCalibration, 0.4f }, { p::osc2Level, 0.8f }, { p::cutoff, 2800.0f },
            { p::ampAttack, 0.05f }, { p::ampRelease, 0.6f }, { p::polyphony, 16.0f } } },
        { "Voice Round-Robin Keys", "DNA", {
            { p::character, 1.0f }, { p::dnaCalibration, 0.3f }, { p::dnaCondition, 0.7f },
            { p::cutoff, 3500.0f }, { p::ampDecay, 0.5f }, { p::ampSustain, 0.4f },
            { p::filterVel, 0.5f } } },
        { "Driven Ladder Bass", "DNA", {
            { p::character, 0.7f }, { p::dnaWarmth, 1.0f }, { p::mixerDrive, 5.0f },
            { p::filterDrive, 14.0f }, { p::cutoff, 420.0f }, { p::resonance, 0.5f },
            { p::subLevel, 0.5f }, { p::polyphony, 1.0f }, { p::filtDecay, 0.25f },
            { p::filterEnvAmt, 0.5f } } },
        { "Soft Supply Pad", "DNA", {
            { p::character, 0.9f }, { p::dnaSupply, 0.05f }, { p::dnaWarmth, 0.5f },
            { p::osc2Level, 0.9f }, { p::osc2Fine, 7.0f }, { p::cutoff, 1800.0f },
            { p::ampAttack, 0.4f }, { p::ampRelease, 1.0f }, { p::polyphony, 16.0f },
            { p::unisonCount, 2.0f } } },
        { "Worn Calibration Brass", "DNA", {
            { p::character, 1.0f }, { p::dnaCondition, 1.0f }, { p::dnaCalibration, 0.1f },
            { p::dnaAge, 0.7f }, { p::osc2Level, 0.85f }, { p::cutoff, 900.0f },
            { p::filterEnvAmt, 0.45f }, { p::filtSustain, 0.4f }, { p::ampAttack, 0.05f } } },
        { "Clean vs Saturated Seq", "DNA", {
            { p::character, 0.6f }, { p::dnaWarmth, 0.95f }, { p::arpMode, 1.0f },
            { p::arpRate, 8.0f }, { p::arpOctaves, 2.0f }, { p::arpGate, 0.45f },
            { p::cutoff, 1000.0f }, { p::resonance, 0.4f }, { p::filterEnvAmt, 0.5f },
            { p::filtDecay, 0.15f }, { p::filtSustain, 0.0f }, { p::ampSustain, 0.5f } } },
        { "Maximum DNA Demo", "DNA", {
            { p::character, 1.0f }, { p::dnaCondition, 1.0f }, { p::dnaCalibration, 0.0f },
            { p::dnaWarmth, 1.0f }, { p::dnaSupply, 0.0f }, { p::dnaAge, 1.0f },
            { p::driftAmount, 6.0f }, { p::warmup, 1.0f }, { p::osc2Level, 0.9f },
            { p::cutoff, 2000.0f }, { p::polyphony, 16.0f } } },

        // ---- ARP ----
        { "Berlin School", "ARP", {
            { p::arpMode, 3.0f }, { p::arpRate, 6.0f }, { p::arpOctaves, 2.0f },
            { p::arpGate, 0.55f }, { p::subLevel, 0.4f }, { p::cutoff, 1100.0f },
            { p::resonance, 0.45f }, { p::filterEnvAmt, 0.5f }, { p::filtDecay, 0.15f },
            { p::filtSustain, 0.2f }, { p::delayMix, 0.35f }, { p::delayTime, 0.375f },
            { p::delayFeedback, 0.55f }, { p::character, 0.7f }, { p::ampRelease, 0.15f } } },
        { "Disco Octaves", "ARP", {
            { p::arpMode, 1.0f }, { p::arpRate, 9.0f }, { p::arpOctaves, 2.0f },
            { p::arpGate, 0.4f }, { p::osc2Wave, 2.0f }, { p::osc2Semi, -12.0f },
            { p::osc2Level, 0.6f }, { p::cutoff, 1400.0f }, { p::filterEnvAmt, 0.6f },
            { p::filtDecay, 0.12f }, { p::filtSustain, 0.0f }, { p::ampSustain, 0.4f },
            { p::ampRelease, 0.1f }, { p::delayMix, 0.2f } } },
        { "Space Whistle Arp", "ARP", {
            { p::arpMode, 4.0f }, { p::arpRate, 10.0f }, { p::arpOctaves, 3.0f },
            { p::arpGate, 0.3f }, { p::osc1Level, 0.0f }, { p::noiseLevel, 0.06f },
            { p::cutoff, 880.0f }, { p::resonance, 1.0f }, { p::keyTrack, 1.0f },
            { p::filterEnvAmt, 0.0f }, { p::ampAttack, 0.01f }, { p::ampRelease, 0.3f },
            { p::reverbMix, 0.5f }, { p::delayMix, 0.3f }, { p::quality, 2.0f } } },
        { "Laser Zap", "ARP", {
            { p::osc1Level, 0.0f }, { p::noiseLevel, 0.04f }, { p::cutoff, 6000.0f },
            { p::resonance, 1.0f }, { p::keyTrack, 1.0f }, { p::filterEnvAmt, 0.9f },
            { p::filtAttack, 0.001f }, { p::filtDecay, 0.2f }, { p::filtSustain, 0.0f },
            { p::ampDecay, 0.25f }, { p::ampSustain, 0.0f }, { p::ampRelease, 0.2f },
            { p::arpMode, 2.0f }, { p::arpRate, 7.0f }, { p::arpOctaves, 2.0f },
            { p::arpGate, 0.35f }, { p::quality, 2.0f }, { p::delayMix, 0.25f } } },
    };
    return presets;
}

} // namespace

SappSynthEditor::SappSynthEditor(SappSynthProcessor& proc)
    : juce::AudioProcessorEditor(proc),
      processor(proc),
      labPanel(proc),
      keyboard(proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);
    namespace p = param;
    sections.reserve(20); // addSection returns references — no reallocation

    // Row 1
    auto& osc1 = addSection("OSCILLATOR 1");
    osc1.slots = { &addChooser(p::osc1Wave, "Wave").box, &addKnob(p::osc1Octave, "Octave").slider,
                   &addKnob(p::osc1Semi, "Semi").slider, &addKnob(p::osc1Fine, "Fine").slider,
                   &addKnob(p::osc1Pw, "Width").slider, &addKnob(p::osc1Level, "Level").slider };
    auto& osc2 = addSection("OSCILLATOR 2");
    osc2.slots = { &addChooser(p::osc2Wave, "Wave").box, &addKnob(p::osc2Octave, "Octave").slider,
                   &addKnob(p::osc2Semi, "Semi").slider, &addKnob(p::osc2Fine, "Fine").slider,
                   &addKnob(p::osc2Pw, "Width").slider, &addKnob(p::osc2Fm, "FM > 1").slider,
                   &addKnob(p::osc2Level, "Level").slider };
    auto& subSection = addSection("SUB + NOISE");
    subSection.slots = { &addChooser(p::subOctave, "Sub Oct").box, &addChooser(p::subWave, "Sub Wave").box,
                         &addKnob(p::subLevel, "Sub").slider, &addKnob(p::noiseLevel, "Noise").slider };
    auto& mixerSection = addSection("MIXER");
    mixerSection.slots = { &addKnob(p::mixerDrive, "Drive").slider,
                           &addKnob(p::mixerChar, "Character").slider };

    // Row 2
    auto& filterSection = addSection("LADDER FILTER");
    filterSection.slots = { &addKnob(p::cutoff, "Cutoff").slider, &addKnob(p::resonance, "Emphasis").slider,
                            &addKnob(p::filterDrive, "Drive").slider, &addKnob(p::keyTrack, "Kbd Trk").slider,
                            &addKnob(p::filterEnvAmt, "Contour").slider, &addKnob(p::filterVel, "Vel").slider };
    auto& ampEnvSection = addSection("LOUDNESS CONTOUR");
    ampEnvSection.slots = { &addKnob(p::ampAttack, "Attack").slider, &addKnob(p::ampDecay, "Decay").slider,
                            &addKnob(p::ampSustain, "Sustain").slider, &addKnob(p::ampRelease, "Release").slider };
    auto& filtEnvSection = addSection("FILTER CONTOUR");
    filtEnvSection.slots = { &addKnob(p::filtAttack, "Attack").slider, &addKnob(p::filtDecay, "Decay").slider,
                             &addKnob(p::filtSustain, "Sustain").slider, &addKnob(p::filtRelease, "Release").slider };
    auto& lfoSection = addSection("LFO");
    lfoSection.slots = { &addChooser(p::lfoShape, "Shape").box, &addKnob(p::lfoRate, "Rate").slider,
                         &addKnob(p::lfoToPitch, "To Pitch").slider, &addKnob(p::lfoToCutoff, "To Filter").slider };

    // Row 3
    auto& analogSection = addSection("ANALOG DNA");
    analogSection.slots = { &addKnob(p::character, "DNA").slider, &addKnob(p::dnaCondition, "Condition").slider,
                            &addKnob(p::dnaCalibration, "Calibrate").slider, &addKnob(p::dnaWarmth, "Warmth").slider,
                            &addKnob(p::dnaSupply, "Supply").slider, &addKnob(p::dnaAge, "Age").slider };
    auto& voiceSection = addSection("VOICES");
    voiceSection.slots = { &addKnob(p::polyphony, "Poly").slider, &addKnob(p::unisonCount, "Unison").slider,
                           &addKnob(p::unisonDetune, "Detune").slider, &addKnob(p::unisonSpread, "Spread").slider,
                           &addKnob(p::glide, "Glide").slider };
    auto& arpSection = addSection("ARPEGGIATOR");
    arpSection.slots = { &addChooser(p::arpMode, "Mode").box, &addKnob(p::arpRate, "Rate").slider,
                         &addKnob(p::arpOctaves, "Octaves").slider, &addKnob(p::arpGate, "Gate").slider };

    // Row 4 (small black knobs) + Lab
    auto& outSection = addSection("OUTPUT");
    outSection.slots = { &addKnob(p::outputDrive, "Drive", true).slider,
                         &addKnob(p::master, "Volume", true).slider,
                         &addChooser(p::quality, "Quality").box };
    auto& chorusSection = addSection("CHORUS");
    chorusSection.slots = { &addKnob(p::chorusMix, "Amount", true).slider,
                            &addKnob(p::chorusRate, "Rate", true).slider };
    auto& delaySection = addSection("ECHO");
    delaySection.slots = { &addKnob(p::delayMix, "Amount", true).slider,
                           &addKnob(p::delayTime, "Time", true).slider,
                           &addKnob(p::delayFeedback, "Regen", true).slider };
    auto& reverbSection = addSection("REVERB");
    reverbSection.slots = { &addKnob(p::reverbMix, "Amount", true).slider,
                            &addKnob(p::reverbSize, "Size", true).slider };
    auto& labSection = addSection("LABORATORY");
    labSection.slots = { &labPanel };
    addAndMakeVisible(labPanel);

    // Header widgets
    presetBox.setTextWhenNothingSelected("PRESETS");
    {
        // Item IDs map to preset indices (+1); section headings carry no ID,
        // so selection stays stable with grouped menus.
        juce::String lastCategory;
        int id = 1;
        for (const auto& preset : factoryPresets())
        {
            if (lastCategory != preset.category)
            {
                lastCategory = preset.category;
                presetBox.addSectionHeading(lastCategory);
            }
            presetBox.addItem(preset.name, id++);
        }
    }
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0)
            applyPreset(id - 1);
    };
    addAndMakeVisible(presetBox);

    newUnitButton.onClick = [this]
    {
        processor.rerollUnitSeed();
        refreshSeedLabel();
    };
    addAndMakeVisible(newUnitButton);

    lockButton.setClickingTogglesState(true);
    lockButton.setToggleState(processor.isSeedLocked(), juce::dontSendNotification);
    lockButton.setTooltip("Lock the unit identity: NEW UNIT is disabled while locked");
    lockButton.onClick = [this]
    {
        processor.setSeedLocked(lockButton.getToggleState());
        newUnitButton.setEnabled(!processor.isSeedLocked());
    };
    newUnitButton.setEnabled(!processor.isSeedLocked());
    addAndMakeVisible(lockButton);

    seedLabel.setJustificationType(juce::Justification::centredRight);
    seedLabel.setFont(vintageFont(10.0f));
    refreshSeedLabel();
    addAndMakeVisible(seedLabel);

    keyboard.setKeyWidth(21.0f);
    keyboard.setAvailableRange(24, 108);
    addAndMakeVisible(keyboard);

    processor.synthEngine().telemetry().setEnabled(true);
    setSize(1220, 700);
}

SappSynthEditor::~SappSynthEditor()
{
    processor.synthEngine().telemetry().setEnabled(false);
    setLookAndFeel(nullptr);
}

SappSynthEditor::Knob& SappSynthEditor::addKnob(const char* paramId, const juce::String& text, bool small)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob->slider.setPopupDisplayEnabled(true, true, nullptr);
    // Granular control: long drag throw, cmd/ctrl-drag for ultra-fine,
    // double-click returns to the parameter default.
    knob->slider.setMouseDragSensitivity(320);
    knob->slider.setVelocityModeParameters(0.6, 1, 0.06, true);
    if (auto* parameter = processor.apvts.getParameter(paramId))
        knob->slider.setDoubleClickReturnValue(
            true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    if (small)
        knob->slider.getProperties().set("small", true);
    knob->label.setText(text.toUpperCase(), juce::dontSendNotification);
    knob->label.setJustificationType(juce::Justification::centred);
    knob->label.setFont(vintageFont(9.5f, true));
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
    // ComboBoxAttachment maps by item index — the items themselves come from
    // the AudioParameterChoice.
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(paramId)))
        chooser->box.addItemList(choice->choices, 1);
    chooser->label.setText(text.toUpperCase(), juce::dontSendNotification);
    chooser->label.setJustificationType(juce::Justification::centred);
    chooser->label.setFont(vintageFont(9.5f, true));
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

    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            withId->setValueNotifyingHost(withId->getDefaultValue());

    for (const auto& [id, value] : presets[static_cast<std::size_t>(index)].values)
        if (auto* parameter = processor.apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void SappSynthEditor::refreshSeedLabel()
{
    seedLabel.setText("UNIT No. " + processor.unitSeedText(), juce::dontSendNotification);
}

void SappSynthEditor::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds();
    const int cheek = 26;

    // Panel: tiled crinkle texture.
    g.setTiledImageFill(lookAndFeel.panelImage, 0, 0, 1.0f);
    g.fillRect(full);

    // Walnut cheeks left/right.
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(lookAndFeel.woodImage, 0, 0, cheek, full.getHeight(),
                0, 0, lookAndFeel.woodImage.getWidth() / 2, lookAndFeel.woodImage.getHeight());
    g.drawImage(lookAndFeel.woodImage, full.getWidth() - cheek, 0, cheek, full.getHeight(),
                lookAndFeel.woodImage.getWidth() / 2, 0,
                lookAndFeel.woodImage.getWidth() / 2, lookAndFeel.woodImage.getHeight());
    // Cheek inner shadow.
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f),
                                           static_cast<float>(cheek), 0.0f,
                                           juce::Colours::transparentBlack,
                                           static_cast<float>(cheek + 10), 0.0f, false));
    g.fillRect(cheek, 0, 10, full.getHeight());
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f),
                                           static_cast<float>(full.getWidth() - cheek), 0.0f,
                                           juce::Colours::transparentBlack,
                                           static_cast<float>(full.getWidth() - cheek - 10), 0.0f, false));
    g.fillRect(full.getWidth() - cheek - 10, 0, 10, full.getHeight());

    // Header: logo plate + pilot lamp.
    g.setColour(colours::cream);
    g.setFont(juce::Font(juce::FontOptions("Georgia", 27.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText("SappSynth", cheek + 16, 8, 240, 30, juce::Justification::centredLeft);
    g.setColour(colours::creamDim);
    g.setFont(vintageFont(9.0f, true));
    juce::String subtitle("MODEL SPSY-1  -  VIRTUAL ANALOG LABORATORY");
    g.drawText(subtitle, cheek + 18, 36, 340, 12, juce::Justification::centredLeft);

    // Amber pilot lamp.
    const float lampX = static_cast<float>(getWidth() - cheek - 24), lampY = 22.0f;
    g.setGradientFill(juce::ColourGradient(colours::amber.brighter(0.6f), lampX, lampY,
                                           colours::amber.withAlpha(0.0f), lampX + 11, lampY + 11, true));
    g.fillEllipse(lampX - 11, lampY - 11, 22, 22);
    g.setColour(colours::amber.brighter(0.8f));
    g.fillEllipse(lampX - 4, lampY - 4, 8, 8);
    g.setColour(juce::Colour(0xff101113));
    g.drawEllipse(lampX - 6.5f, lampY - 6.5f, 13, 13, 2.0f);

    // Section plates: engraved border + silk-screen title.
    for (const auto& section : sections)
    {
        const auto r = section.bounds.toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(colours::engrave);
        g.drawRoundedRectangle(r, 4.0f, 1.2f);
        g.setColour(colours::engraveHi.withAlpha(0.35f));
        g.drawRoundedRectangle(r.translated(0.0f, 1.0f), 4.0f, 0.6f);

        g.setColour(colours::cream.withAlpha(0.85f));
        g.setFont(vintageFont(10.0f, true));
        g.drawText(section.title, section.bounds.getX(), section.bounds.getY() + 4,
                   section.bounds.getWidth(), 12, juce::Justification::centred);
        // engraved rule under the title
        g.setColour(colours::engrave);
        const int cy = section.bounds.getY() + 18;
        g.drawLine(static_cast<float>(section.bounds.getX() + 10), static_cast<float>(cy),
                   static_cast<float>(section.bounds.getRight() - 10), static_cast<float>(cy), 1.0f);
    }

    // Corner screws.
    const auto& screw = lookAndFeel.screwImage;
    if (screw.isValid())
    {
        const int s = 14;
        for (const auto& pt : { juce::Point<int>(cheek + 6, 6),
                                juce::Point<int>(getWidth() - cheek - 6 - s, 6),
                                juce::Point<int>(cheek + 6, getHeight() - 6 - s),
                                juce::Point<int>(getWidth() - cheek - 6 - s, getHeight() - 6 - s) })
            g.drawImage(screw, pt.x, pt.y, s, s, 0, 0, screw.getWidth(), screw.getHeight());
    }
}

void SappSynthEditor::layoutRow(const std::vector<int>& sectionIndices, juce::Rectangle<int> rowArea)
{
    auto effectiveSlots = [](const Section& s)
    {
        // The Lab display earns the width of five knobs.
        if (s.slots.size() == 1 && dynamic_cast<LabPanel*>(s.slots.front()) != nullptr)
            return 5;
        return static_cast<int>(std::max<std::size_t>(1, s.slots.size()));
    };

    int totalSlots = 0;
    for (const int sectionIndex : sectionIndices)
        totalSlots += effectiveSlots(sections[static_cast<std::size_t>(sectionIndex)]);

    const int sectionGap = 8;
    const int gaps = (static_cast<int>(sectionIndices.size()) - 1) * sectionGap;
    const float slotWidth = static_cast<float>(rowArea.getWidth() - gaps) / static_cast<float>(totalSlots);

    int x = rowArea.getX();
    for (const int sectionIndex : sectionIndices)
    {
        auto& section = sections[static_cast<std::size_t>(sectionIndex)];
        const int slotCount = static_cast<int>(std::max<std::size_t>(1, section.slots.size()));
        const int width = static_cast<int>(slotWidth * static_cast<float>(effectiveSlots(section)));
        section.bounds = { x, rowArea.getY(), width, rowArea.getHeight() };

        auto inner = section.bounds.reduced(6).withTrimmedTop(18);

        if (section.slots.size() == 1 && dynamic_cast<LabPanel*>(section.slots.front()) != nullptr)
        {
            section.slots.front()->setBounds(inner);
        }
        else
        {
            const int cellWidth = inner.getWidth() / slotCount;
            for (auto* slot : section.slots)
            {
                auto cell = inner.removeFromLeft(cellWidth);
                if (auto* box = dynamic_cast<juce::ComboBox*>(slot))
                {
                    box->setBounds(cell.removeFromTop(cell.getHeight() - 14)
                                       .withSizeKeepingCentre(cell.getWidth() - 4, 24));
                    for (const auto& chooser : choosers)
                        if (&chooser->box == box)
                            chooser->label.setBounds(cell);
                }
                else if (auto* slider = dynamic_cast<juce::Slider*>(slot))
                {
                    slider->setBounds(cell.removeFromTop(cell.getHeight() - 14));
                    for (const auto& knob : knobs)
                        if (&knob->slider == slider)
                            knob->label.setBounds(cell);
                }
            }
        }
        x += width + sectionGap;
    }
}

void SappSynthEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft(26 + 8);   // wood cheek + breathing room
    area.removeFromRight(26 + 8);
    area.removeFromTop(6);
    area.removeFromBottom(8);

    auto header = area.removeFromTop(44);
    header.removeFromLeft(360); // logo zone (painted)
    header.removeFromRight(40); // pilot lamp zone
    seedLabel.setBounds(header.removeFromRight(130));
    lockButton.setBounds(header.removeFromRight(52).reduced(0, 9));
    header.removeFromRight(4);
    newUnitButton.setBounds(header.removeFromRight(88).reduced(0, 9));
    header.removeFromRight(8);
    presetBox.setBounds(header.removeFromRight(200).reduced(0, 9));

    keyboard.setBounds(area.removeFromBottom(76));
    area.removeFromBottom(8);

    const int rowGap = 8;
    const int rowHeight = (area.getHeight() - 3 * rowGap) / 4;
    auto takeRow = [&]
    {
        auto r = area.removeFromTop(rowHeight);
        area.removeFromTop(rowGap);
        return r;
    };
    layoutRow({ 0, 1, 2, 3 }, takeRow());
    layoutRow({ 4, 5, 6, 7 }, takeRow());
    layoutRow({ 8, 9, 10 }, takeRow());
    layoutRow({ 11, 12, 13, 14, 15 }, takeRow());
}

} // namespace sappsynth
