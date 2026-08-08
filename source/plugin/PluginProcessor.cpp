#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../parameters/ParameterIds.h"
#include "FactoryPresets.h"
#include "SappLinkCCMap.h"

namespace sappsynth {

namespace {

juce::NormalisableRange<float> logRange(float lo, float hi)
{
    juce::NormalisableRange<float> range(lo, hi);
    range.setSkewForCentre(std::sqrt(lo * hi));
    return range;
}

juce::NormalisableRange<float> timeRange()
{
    juce::NormalisableRange<float> range(0.001f, 5.0f);
    range.setSkewForCentre(0.3f);
    return range;
}

} // namespace

SappSynthProcessor::SappSynthProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    eventScratch.reserve(1024);
    engine.setUnitSeed(0x5A995EEDull);

    const auto& table = sapplink::mappings();
    for (std::size_t i = 0; i < table.size(); ++i)
        ccSlews[i].parameter = apvts.getParameter(table[i].paramId);

    // Host-automatable sound selection (sapptune issue #13). The callback can
    // arrive on the audio thread; it only stores an index.
    apvts.addParameterListener(sapp::userpresets::kPresetParamId, this);

    startTimerHz(30);   // deferred program-change apply (message thread)
}

SappSynthProcessor::~SappSynthProcessor()
{
    apvts.removeParameterListener(sapp::userpresets::kPresetParamId, this);
}

// ------------------------------------------------------- factory programs --

int SappSynthProcessor::getNumPrograms()
{
    return static_cast<int>(presets::all().size());
}

const juce::String SappSynthProcessor::getProgramName(int index)
{
    const auto& bank = presets::all();
    if (index < 0 || index >= static_cast<int>(bank.size()))
        return {};
    return bank[static_cast<std::size_t>(index)].name;
}

void SappSynthProcessor::setCurrentProgram(int index)
{
    // Hosts may call this from any thread; defer to the timer like a MIDI
    // program change. currentProgram updates immediately so hosts that read
    // it straight back see the new value.
    if (index < 0 || index >= getNumPrograms() || index == currentProgram.load())
        return;
    currentProgram.store(index);
    pendingProgram.store(index);
}

void SappSynthProcessor::applyFactoryPreset(int index)
{
    const auto& bank = presets::all();
    if (index < 0 || index >= static_cast<int>(bank.size()))
        return;

    for (auto* parameter : getParameters())
        if (auto* withId = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            // `preset` is the chooser, not part of the sound: resetting it
            // here would snap the selection back to program 0 on every load.
            if (withId->paramID != sapp::userpresets::kPresetParamId)
                withId->setValueNotifyingHost(withId->getDefaultValue());

    for (const auto& [id, value] : bank[static_cast<std::size_t>(index)].values)
        if (auto* parameter = apvts.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));

    currentProgram.store(index);
    syncPresetParameter(index);
    updateHostDisplay(ChangeDetails {}.withProgramChanged(true));
}

// ---------------------------------------------------------- user presets --

int SappSynthProcessor::factoryPresetCount() const
{
    return static_cast<int>(presets::all().size());
}

std::vector<sapp::userpresets::UserPreset> SappSynthProcessor::userPresets() const
{
    return sapp::userpresets::scan(kInstrument);
}

bool SappSynthProcessor::saveUserPreset(const juce::String& name, const juce::String& notes,
                                        juce::String& error)
{
    auto preset = sapp::userpresets::capture(*this, name.trim(), notes);
    juce::File written;
    return sapp::userpresets::save(preset, kInstrument, written, error);
}

bool SappSynthProcessor::loadUserPreset(const juce::String& name, juce::String& error)
{
    const auto preset = sapp::userpresets::findByName(kInstrument, name);
    if (!preset.has_value())
    {
        error = "no user preset named \"" + name + "\" in "
                + sapp::userpresets::presetDir(kInstrument).getFullPathName();
        return false;
    }
    sapp::userpresets::apply(*preset, apvts);
    return true;
}

void SappSynthProcessor::applyPresetChoice(int index)
{
    if (index < 0)
        return;
    if (index < factoryPresetCount())
    {
        applyFactoryPreset(index);
        return;
    }
    // Beyond the factory bank: resolve the choice label back to a name and
    // load from disk, so the file is the source of truth even if it changed
    // since this instance was constructed.
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
        apvts.getParameter(sapp::userpresets::kPresetParamId));
    if (choice == nullptr || index >= choice->choices.size())
        return;
    juce::String error;
    loadUserPreset(sapp::userpresets::nameFromChoiceLabel(choice->choices[index]), error);
    syncPresetParameter(index);
}

void SappSynthProcessor::syncPresetParameter(int choiceIndex)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
        apvts.getParameter(sapp::userpresets::kPresetParamId));
    if (choice == nullptr || choiceIndex < 0 || choiceIndex >= choice->choices.size())
        return;
    if (choice->getIndex() == choiceIndex)
        return;
    const juce::ScopedValueSetter<bool> guard(applyingPreset, true);
    choice->setValueNotifyingHost(choice->convertTo0to1(static_cast<float>(choiceIndex)));
}

void SappSynthProcessor::parameterChanged(const juce::String& parameterId, float newValue)
{
    if (applyingPreset || parameterId != sapp::userpresets::kPresetParamId)
        return;
    pendingPresetChoice.store(static_cast<int>(newValue));
}

void SappSynthProcessor::timerCallback()
{
    const int program = pendingProgram.exchange(-1);
    if (program >= 0)
        applyFactoryPreset(program);

    const int choice = pendingPresetChoice.exchange(-1);
    if (choice >= 0)
        applyPresetChoice(choice);
}

void SappSynthProcessor::handleSappLinkCc(int ccNumber, int ccValue)
{
    const auto* mapping = sapplink::findMapping(ccNumber);
    if (mapping == nullptr)
        return;
    const auto index = static_cast<std::size_t>(mapping - sapplink::mappings().data());
    auto& slew = ccSlews[index];
    if (slew.parameter == nullptr)
        return;
    slew.target = slew.parameter->convertTo0to1(sapplink::ccToEngineering(*mapping, ccValue));
    if (!slew.active)
        slew.current = slew.parameter->getValue();
    slew.active = true;
}

void SappSynthProcessor::advanceCcSlews(int numSamples, double sampleRate)
{
    // ~15 ms approach per step; applied through the same normalized-value
    // path host automation uses, never straight into the DSP.
    const float coefficient = 1.0f - std::exp(-static_cast<float>(numSamples)
                                              / (0.015f * static_cast<float>(sampleRate)));
    for (auto& slew : ccSlews)
    {
        if (!slew.active || slew.parameter == nullptr)
            continue;
        slew.current += (slew.target - slew.current) * coefficient;
        if (std::abs(slew.target - slew.current) < 1.0e-4f)
        {
            slew.current = slew.target;
            slew.active = false;
        }
        slew.parameter->setValueNotifyingHost(slew.current);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SappSynthProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using Pc = juce::AudioParameterChoice;
    using Pi = juce::AudioParameterInt;
    using ID = juce::ParameterID;
    namespace p = param;

    const juce::StringArray waves { "Sine", "Saw", "Pulse", "Tri" };
    const juce::StringArray lfoShapes { "Sine", "Tri", "Saw", "Sqr" };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto oscGroup = [&](const char* wave, const char* oct, const char* semi,
                        const char* fine, const char* pw, const char* level,
                        const juce::String& prefix, int defaultWave, float defaultLevel)
    {
        layout.add(std::make_unique<Pc>(ID{wave, 1}, prefix + " Wave", waves, defaultWave));
        layout.add(std::make_unique<Pi>(ID{oct, 1}, prefix + " Octave", -2, 2, 0));
        layout.add(std::make_unique<Pi>(ID{semi, 1}, prefix + " Semi", -12, 12, 0));
        layout.add(std::make_unique<P>(ID{fine, 1}, prefix + " Fine",
                                       juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f));
        layout.add(std::make_unique<P>(ID{pw, 1}, prefix + " PW",
                                       juce::NormalisableRange<float>(0.05f, 0.95f), 0.5f));
        layout.add(std::make_unique<P>(ID{level, 1}, prefix + " Level",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), defaultLevel));
    };
    oscGroup(p::osc1Wave, p::osc1Octave, p::osc1Semi, p::osc1Fine, p::osc1Pw, p::osc1Level, "Osc 1", 1, 1.0f);
    oscGroup(p::osc2Wave, p::osc2Octave, p::osc2Semi, p::osc2Fine, p::osc2Pw, p::osc2Level, "Osc 2", 1, 0.0f);
    layout.add(std::make_unique<P>(ID{p::osc2Fm, 1}, "Osc2>1 FM",
                                   juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 0.6f), 0.0f));

    layout.add(std::make_unique<Pc>(ID{p::subOctave, 1}, "Sub Octave", juce::StringArray { "-1", "-2" }, 0));
    layout.add(std::make_unique<Pc>(ID{p::subWave, 1}, "Sub Wave", juce::StringArray { "Sqr", "Sine" }, 0));
    layout.add(std::make_unique<P>(ID{p::subLevel, 1}, "Sub Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<P>(ID{p::noiseLevel, 1}, "Noise Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<P>(ID{p::mixerDrive, 1}, "Mix Drive", logRange(1.0f, 8.0f), 1.2f));
    layout.add(std::make_unique<P>(ID{p::mixerChar, 1}, "Mix Character", juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));

    layout.add(std::make_unique<P>(ID{p::cutoff, 1}, "Cutoff", logRange(20.0f, 20000.0f), 9000.0f));
    layout.add(std::make_unique<P>(ID{p::resonance, 1}, "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    layout.add(std::make_unique<P>(ID{p::filterDrive, 1}, "Filter Drive",
                                   juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID{p::keyTrack, 1}, "Key Track", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<P>(ID{p::filterEnvAmt, 1}, "Env Amount",
                                   juce::NormalisableRange<float>(-1.0f, 1.0f), 0.35f));
    layout.add(std::make_unique<P>(ID{p::filterVel, 1}, "Velocity", juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    auto adsr = [&](const char* a, const char* d, const char* s, const char* r,
                    const juce::String& prefix, float da, float dd, float ds, float dr)
    {
        layout.add(std::make_unique<P>(ID{a, 1}, prefix + " Attack", timeRange(), da));
        layout.add(std::make_unique<P>(ID{d, 1}, prefix + " Decay", timeRange(), dd));
        layout.add(std::make_unique<P>(ID{s, 1}, prefix + " Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), ds));
        layout.add(std::make_unique<P>(ID{r, 1}, prefix + " Release", timeRange(), dr));
    };
    adsr(p::ampAttack, p::ampDecay, p::ampSustain, p::ampRelease, "Amp", 0.005f, 0.2f, 0.8f, 0.25f);
    adsr(p::filtAttack, p::filtDecay, p::filtSustain, p::filtRelease, "Filter", 0.005f, 0.25f, 0.3f, 0.25f);

    layout.add(std::make_unique<P>(ID{p::lfoRate, 1}, "LFO Rate", logRange(0.02f, 20.0f), 1.5f));
    layout.add(std::make_unique<Pc>(ID{p::lfoShape, 1}, "LFO Shape", lfoShapes, 0));
    layout.add(std::make_unique<P>(ID{p::lfoToPitch, 1}, "LFO>Pitch",
                                   juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID{p::lfoToCutoff, 1}, "LFO>Cutoff", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<Pi>(ID{p::polyphony, 1}, "Polyphony", 1, 16, 8));
    layout.add(std::make_unique<Pi>(ID{p::unisonCount, 1}, "Unison", 1, 5, 1));
    layout.add(std::make_unique<P>(ID{p::unisonDetune, 1}, "Uni Detune",
                                   juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 12.0f));
    layout.add(std::make_unique<P>(ID{p::unisonSpread, 1}, "Uni Spread", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.add(std::make_unique<P>(ID{p::glide, 1}, "Glide",
                                   juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));

    layout.add(std::make_unique<Pc>(ID{p::arpMode, 1}, "Arp Mode",
                                    juce::StringArray { "Off", "Up", "Down", "UpDown", "Random" }, 0));
    layout.add(std::make_unique<P>(ID{p::arpRate, 1}, "Arp Rate", logRange(0.5f, 20.0f), 8.0f));
    layout.add(std::make_unique<Pi>(ID{p::arpOctaves, 1}, "Arp Octaves", 1, 3, 1));
    layout.add(std::make_unique<P>(ID{p::arpGate, 1}, "Arp Gate",
                                   juce::NormalisableRange<float>(0.05f, 0.95f), 0.5f));

    layout.add(std::make_unique<P>(ID{p::chorusMix, 1}, "Chorus", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<P>(ID{p::chorusRate, 1}, "Chorus Rate", logRange(0.05f, 4.0f), 0.5f));
    layout.add(std::make_unique<P>(ID{p::delayTime, 1}, "Delay Time",
                                   juce::NormalisableRange<float>(0.05f, 1.2f, 0.001f, 0.5f), 0.35f));
    layout.add(std::make_unique<P>(ID{p::delayFeedback, 1}, "Delay FB", juce::NormalisableRange<float>(0.0f, 0.85f), 0.35f));
    layout.add(std::make_unique<P>(ID{p::delayMix, 1}, "Delay", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<P>(ID{p::reverbSize, 1}, "Verb Size", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<P>(ID{p::reverbMix, 1}, "Reverb", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<P>(ID{p::character, 1}, "DNA Amount", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<P>(ID{p::dnaCondition, 1}, "Condition", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f));
    layout.add(std::make_unique<P>(ID{p::dnaCalibration, 1}, "Calibration", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.add(std::make_unique<P>(ID{p::dnaWarmth, 1}, "Warmth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));
    layout.add(std::make_unique<P>(ID{p::dnaSupply, 1}, "Supply", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.add(std::make_unique<P>(ID{p::dnaAge, 1}, "Age", juce::NormalisableRange<float>(0.0f, 1.0f), 0.25f));
    layout.add(std::make_unique<P>(ID{p::driftAmount, 1}, "Drift",
                                   juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 1.5f));
    layout.add(std::make_unique<P>(ID{p::driftSpeed, 1}, "Drift Speed", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<P>(ID{p::warmup, 1}, "Warm-up", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<P>(ID{p::outputDrive, 1}, "Out Drive",
                                   juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID{p::master, 1}, "Master",
                                   juce::NormalisableRange<float>(-40.0f, 6.0f, 0.1f), -6.0f));
    layout.add(std::make_unique<Pc>(ID{p::quality, 1}, "Quality",
                                    juce::StringArray { "Eco", "Normal", "High" }, 1));

    // Host-automatable sound selection (sapptune issue #13). ADDED LAST so no
    // existing parameter's index moves — automation lanes are a contract.
    // The factory bank in program order, then the user presets that exist
    // right now; the list is fixed for this instance's lifetime because a
    // choice parameter cannot change its choices without breaking lanes.
    juce::StringArray presetChoices;
    for (const auto& preset : presets::all())
        presetChoices.add(preset.name);
    presetChoices.addArray(sapp::userpresets::choiceLabels(SappSynthProcessor::kInstrument));
    layout.add(std::make_unique<Pc>(ID{sapp::userpresets::kPresetParamId, 1}, "Preset",
                                    presetChoices, 0));

    return layout;
}

PatchState SappSynthProcessor::buildPatchFromParameters()
{
    namespace p = param;
    auto value = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    auto choice = [&](const char* id) { return static_cast<int>(value(id)); };

    PatchState patch;
    auto fillOsc = [&](OscillatorParams& osc, const char* wave, const char* oct,
                       const char* semi, const char* fine, const char* pw, const char* level)
    {
        osc.waveform = static_cast<Waveform>(choice(wave));
        osc.octave = choice(oct);
        osc.semitones = choice(semi);
        osc.fineCents = value(fine);
        osc.pulseWidth = value(pw);
        osc.level = value(level);
    };
    fillOsc(patch.osc1, p::osc1Wave, p::osc1Octave, p::osc1Semi, p::osc1Fine, p::osc1Pw, p::osc1Level);
    fillOsc(patch.osc2, p::osc2Wave, p::osc2Octave, p::osc2Semi, p::osc2Fine, p::osc2Pw, p::osc2Level);

    patch.osc2ToOsc1Fm = value(p::osc2Fm);
    patch.subOctave = choice(p::subOctave) + 1;
    patch.subWaveform = choice(p::subWave) == 0 ? Waveform::Pulse : Waveform::Sine;
    patch.subLevel = value(p::subLevel);
    patch.noiseLevel = value(p::noiseLevel);

    patch.mixerDrive = value(p::mixerDrive);
    patch.mixerCharacter = value(p::mixerChar);

    patch.cutoffHz = value(p::cutoff);
    patch.resonance = value(p::resonance);
    patch.filterDriveDb = value(p::filterDrive);
    patch.keyTrack = value(p::keyTrack);
    patch.filterEnvAmount = value(p::filterEnvAmt);
    patch.velocityToCutoff = value(p::filterVel);

    patch.ampAttack = value(p::ampAttack);
    patch.ampDecay = value(p::ampDecay);
    patch.ampSustain = value(p::ampSustain);
    patch.ampRelease = value(p::ampRelease);
    patch.filterAttack = value(p::filtAttack);
    patch.filterDecay = value(p::filtDecay);
    patch.filterSustain = value(p::filtSustain);
    patch.filterRelease = value(p::filtRelease);

    patch.lfoRateHz = value(p::lfoRate);
    patch.lfoShape = static_cast<LfoShape>(choice(p::lfoShape));
    patch.lfoToPitchCents = value(p::lfoToPitch);
    patch.lfoToCutoff = value(p::lfoToCutoff);

    patch.polyphony = choice(p::polyphony);
    patch.unisonCount = choice(p::unisonCount);
    patch.unisonDetuneCents = value(p::unisonDetune);
    patch.unisonSpread = value(p::unisonSpread);
    patch.glideSeconds = value(p::glide);
    patch.arpMode = choice(p::arpMode);
    patch.arpRateHz = value(p::arpRate);
    patch.arpOctaves = choice(p::arpOctaves);
    patch.arpGate = value(p::arpGate);

    patch.chorusMix = value(p::chorusMix);
    patch.chorusRateHz = value(p::chorusRate);
    patch.delayTimeS = value(p::delayTime);
    patch.delayFeedback = value(p::delayFeedback);
    patch.delayMix = value(p::delayMix);
    patch.reverbSize = value(p::reverbSize);
    patch.reverbMix = value(p::reverbMix);

    patch.characterAmount = value(p::character);
    patch.dnaCondition = value(p::dnaCondition);
    patch.dnaCalibration = value(p::dnaCalibration);
    patch.dnaWarmth = value(p::dnaWarmth);
    patch.dnaSupply = value(p::dnaSupply);
    patch.dnaAge = value(p::dnaAge);
    patch.driftAmountCents = value(p::driftAmount);
    patch.driftSpeed = value(p::driftSpeed);
    patch.warmupAmount = value(p::warmup);

    patch.outputDriveDb = value(p::outputDrive);
    patch.masterDb = value(p::master);
    patch.quality = static_cast<QualityMode>(choice(p::quality));
    return patch;
}

void SappSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.setPatch(buildPatchFromParameters());
    engine.prepare(sampleRate, samplesPerBlock);
}

bool SappSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SappSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    keyboardState.processNextMidiBuffer(midi, 0, numSamples, true);

    eventScratch.clear();
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        Event event;
        event.sampleOffset = metadata.samplePosition;
        if (message.isNoteOn())
        {
            event.type = Event::Type::NoteOn;
            event.note = message.getNoteNumber();
            event.velocity = message.getFloatVelocity();
        }
        else if (message.isNoteOff())
        {
            event.type = Event::Type::NoteOff;
            event.note = message.getNoteNumber();
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            event.type = Event::Type::AllNotesOff;
        }
        else if (message.isController())
        {
            // SappLink CC-in (any channel). CC 1/64 and pitch bend fall
            // through untouched — they are not part of the mapping.
            handleSappLinkCc(message.getControllerNumber(), message.getControllerValue());
            continue;
        }
        else if (message.isProgramChange())
        {
            // Factory-preset select; applied on the message thread (timer).
            pendingProgram.store(message.getProgramChangeNumber());
            continue;
        }
        else
            continue;
        if (eventScratch.size() < eventScratch.capacity())
            eventScratch.push_back(event);
    }
    midi.clear();

    advanceCcSlews(numSamples, getSampleRate());
    engine.setPatch(buildPatchFromParameters());

    RenderBlock block;
    block.left = buffer.getWritePointer(0);
    block.right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);
    block.numSamples = numSamples;
    block.events = std::span<const Event>(eventScratch.data(), eventScratch.size());
    engine.process(block);
}

void SappSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("unitSeed", juce::String(static_cast<juce::uint64>(engine.unitSeed())), nullptr);
    state.setProperty("unitSeedLocked", seedLocked, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SappSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
        {
            apvts.replaceState(state);
            const auto seedText = state.getProperty("unitSeed").toString();
            if (seedText.isNotEmpty())
                engine.setUnitSeed(seedText.getLargeIntValue() > 0
                                       ? static_cast<std::uint64_t>(seedText.getLargeIntValue())
                                       : 0x5A995EEDull);
            seedLocked = static_cast<bool>(state.getProperty("unitSeedLocked", false));
        }
    }
}

void SappSynthProcessor::rerollUnitSeed()
{
    if (!seedLocked)
        engine.setUnitSeed(static_cast<std::uint64_t>(juce::Random::getSystemRandom().nextInt64()));
}

juce::String SappSynthProcessor::unitSeedText() const
{
    return juce::String::toHexString(static_cast<juce::int64>(engine.unitSeed())).toUpperCase();
}

juce::AudioProcessorEditor* SappSynthProcessor::createEditor()
{
    return new SappSynthEditor(*this);
}

} // namespace sappsynth

// JUCE entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sappsynth::SappSynthProcessor();
}
