#include "SynthEngine.h"
#include <algorithm>
#include <cmath>
#include "../dsp/utility/DenormalGuard.h"
#include "../dsp/utility/FastMath.h"

namespace sappsynth {

void SynthEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    sr = sampleRate;

    voiceManager.setUnitSeed(unitSeed_);
    voiceManager.prepare(sr);
    unitProfile_ = UnitProfile::generate(unitSeed_);

    lfo.prepare(sr);
    unitDrift.prepare(sr);
    unitDrift.seed(seeds::combine(unitSeed_, 0x0D21F7ull));
    thermal.prepare(sr);

    smCutoff.prepare(sr / SynthVoice::kControlInterval, 0.010f);
    smResonance.prepare(sr / SynthVoice::kControlInterval, 0.005f);
    smMixerDrive.prepare(sr / SynthVoice::kControlInterval, 0.010f);
    smMaster.prepare(sr, 0.010f); // ticked per sample in the output loop

    chorus.prepare(sr);
    delayFx.prepare(sr);
    reverbFx.prepare(sr);
    smCutoff.reset(patch_.cutoffHz);
    smResonance.reset(patch_.resonance);
    smMixerDrive.reset(patch_.mixerDrive);
    smMaster.reset(dbToGain(patch_.masterDb));

    activeQuality = patch_.quality;
    voiceManager.applyQuality(ProcessingQuality::forMode(activeQuality));

    effectivePatch_ = patch_;
    prepared = true;
}

void SynthEngine::reset()
{
    for (int i = 0; i < VoiceManager::kMaxVoices; ++i)
        voiceManager.voice(i).hardStop();
}

void SynthEngine::setUnitSeed(std::uint64_t seed)
{
    unitSeed_ = seed;
    unitProfile_ = UnitProfile::generate(unitSeed_);
    voiceManager.setUnitSeed(unitSeed_);
    unitDrift.seed(seeds::combine(unitSeed_, 0x0D21F7ull));
}

void SynthEngine::setPatch(const PatchState& patch)
{
    patch_ = patch;
}

void SynthEngine::applyEvent(const Event& event)
{
    switch (event.type)
    {
        case Event::Type::NoteOn:
            if (event.velocity > 0.0f)
            {
                voiceManager.noteOn(event.note, std::clamp(event.velocity, 0.0f, 1.0f),
                                    effectivePatch_, unitProfile_, lastPlayedNote);
                lastPlayedNote = event.note;
            }
            else
                voiceManager.noteOff(event.note);
            break;
        case Event::Type::NoteOff:
            voiceManager.noteOff(event.note);
            break;
        case Event::Type::AllNotesOff:
            voiceManager.allNotesOff();
            break;
    }
}

void SynthEngine::updateControl(int numSamples)
{
    sharedMod.lfoValue = lfo.process(numSamples);
    sharedMod.unitDriftNorm = labDriftFrozen.load(std::memory_order_relaxed)
                            ? unitDrift.value() : unitDrift.process(numSamples);
    sharedMod.warmupCents = thermal.advance(numSamples) * effectivePatch_.warmupAmount;
    sharedMod.characterAmount = effectivePatch_.characterAmount;

    smCutoff.setTarget(patch_.cutoffHz);
    smResonance.setTarget(patch_.resonance);
    smMixerDrive.setTarget(patch_.mixerDrive);

    effectivePatch_ = patch_;
    effectivePatch_.cutoffHz = smCutoff.next();
    effectivePatch_.resonance = smResonance.next();
    effectivePatch_.mixerDrive = smMixerDrive.next();

    // Lab ideal mode: strip every modeled behavior — variation, drift,
    // warm-up, mixer/VCA/output nonlinearity — leaving the clean digital core.
    if (labIdeal.load(std::memory_order_relaxed))
    {
        effectivePatch_.characterAmount = 0.0f;
        effectivePatch_.driftAmountCents = 0.0f;
        effectivePatch_.warmupAmount = 0.0f;
        effectivePatch_.mixerDrive = 1.0f;
        effectivePatch_.mixerCharacter = 0.0f;
        effectivePatch_.outputDriveDb = 0.0f;
    }
    sharedMod.driftFrozen = labDriftFrozen.load(std::memory_order_relaxed);
    sharedMod.characterAmount = effectivePatch_.characterAmount;

    lfo.setRate(patch_.lfoRateHz);
    lfo.setShape(patch_.lfoShape);

    // Drift dynamics: speed knob sets mean-reversion; sigma keeps the
    // stationary standard deviation at 1 so voices scale it to cents.
    const float theta = 0.2f + patch_.driftSpeed * 3.0f;
    const float sigma = std::sqrt(2.0f * theta);
    unitDrift.setParameters(theta, sigma);
    thermal.configure(15.0f, 90.0f, 0.9f);

    outputDriveLinear = dbToGain(effectivePatch_.outputDriveDb);
}

void SynthEngine::renderSpan(float* left, float* right, int numSamples)
{
    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = std::min(SynthVoice::kControlInterval, numSamples - offset);
        updateControl(chunk);

        voiceManager.forEachActiveVoice([&](SynthVoice& voice)
        {
            voice.renderChunk(left + offset, right + offset, chunk,
                              effectivePatch_, unitProfile_, sharedMod);
        });

        // Output drive: soft clip, normalized so drive=1 is transparent until
        // it limits. Effects + master gain run once per block after all spans.
        const float d = outputDriveLinear;
        const float norm = 1.0f / fastTanh(std::max(d, 1.0f) * 0.8f);
        for (int i = offset; i < offset + chunk; ++i)
        {
            left[i]  = fastTanh(left[i] * d * 0.8f) * norm;
            right[i] = fastTanh(right[i] * d * 0.8f) * norm;
        }
        offset += chunk;
    }
}

void SynthEngine::process(const RenderBlock& block)
{
    if (!prepared || block.numSamples <= 0)
        return;

    DenormalGuard guard;

    std::fill(block.left, block.left + block.numSamples, 0.0f);
    std::fill(block.right, block.right + block.numSamples, 0.0f);

    // Quality changes wait for silence: reconfiguring the island/solver while
    // voices sound would click (architecture §12). Held notes keep the old
    // mode; the new one arrives on the next silent block.
    if (patch_.quality != activeQuality && voiceManager.activeVoiceCount() == 0)
    {
        activeQuality = patch_.quality;
        voiceManager.applyQuality(ProcessingQuality::forMode(activeQuality));
    }

    // Event-split rendering (architecture §6.3).
    int cursor = 0;
    for (const auto& event : block.events)
    {
        const int eventSample = std::clamp(event.sampleOffset, 0, block.numSamples);
        if (eventSample > cursor)
        {
            renderSpan(block.left + cursor, block.right + cursor, eventSample - cursor);
            cursor = eventSample;
        }
        applyEvent(event);
    }
    if (cursor < block.numSamples)
        renderSpan(block.left + cursor, block.right + cursor, block.numSamples - cursor);

    // Global FX chain (§7: SUM -> DRIVE -> FX -> OUT), then master gain.
    chorus.setParameters(patch_.chorusMix, patch_.chorusRateHz);
    delayFx.setParameters(patch_.delayTimeS, patch_.delayFeedback, patch_.delayMix);
    reverbFx.setParameters(patch_.reverbSize, patch_.reverbMix);
    chorus.process(block.left, block.right, block.numSamples);
    delayFx.process(block.left, block.right, block.numSamples);
    reverbFx.process(block.left, block.right, block.numSamples);

    smMaster.setTarget(dbToGain(patch_.masterDb));
    for (int i = 0; i < block.numSamples; ++i)
    {
        const float master = smMaster.next();
        block.left[i] *= master;
        block.right[i] *= master;
    }

    telemetryBus.push(block.left, block.right, block.numSamples);
}

} // namespace sappsynth
