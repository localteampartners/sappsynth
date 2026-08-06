#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../parameters/ParameterIds.h"

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
        else
            continue;
        if (eventScratch.size() < eventScratch.capacity())
            eventScratch.push_back(event);
    }
    midi.clear();

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
