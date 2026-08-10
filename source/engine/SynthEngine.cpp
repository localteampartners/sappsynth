#include "SynthEngine.h"
#include <algorithm>
#include <cmath>
#include "../dsp/nonlinear/OutputStage.h"
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
    arpRng.seed(seeds::combine(unitSeed_, 0xA49ull));
    arpHeldCount = 0;
    arpSoundingNote = -1;
    arpStepIndex = 0;
    arpStepCountdown = 0.0;
    arpGateCountdown = 0.0;

    smCutoff.prepare(sr / SynthVoice::kControlInterval, 0.010f);
    smResonance.prepare(sr / SynthVoice::kControlInterval, 0.005f);
    smMixerDrive.prepare(sr / SynthVoice::kControlInterval, 0.010f);
    smMaster.prepare(sr, 0.010f); // ticked per sample in the output loop

    chorus.prepare(sr);
    delayFx.prepare(sr);
    delayFx.setLevelReference(kBusHeadroom);
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

void SynthEngine::arpAddHeld(int note, float velocity)
{
    if (arpHeldCount >= 16)
        return;
    for (int i = 0; i < arpHeldCount; ++i)
        if (arpHeld[i].note == note)
            return;
    // Keep sorted by pitch so Up/Down patterns are stable.
    int pos = arpHeldCount;
    while (pos > 0 && arpHeld[pos - 1].note > note)
    {
        arpHeld[pos] = arpHeld[pos - 1];
        --pos;
    }
    arpHeld[pos] = { note, velocity };
    ++arpHeldCount;
}

void SynthEngine::arpRemoveHeld(int note)
{
    for (int i = 0; i < arpHeldCount; ++i)
        if (arpHeld[i].note == note)
        {
            for (int j = i; j < arpHeldCount - 1; ++j)
                arpHeld[j] = arpHeld[j + 1];
            --arpHeldCount;
            return;
        }
}

void SynthEngine::applyEvent(const Event& event)
{
    const bool arpOn = effectivePatch_.arpMode > 0;
    switch (event.type)
    {
        case Event::Type::NoteOn:
            if (event.velocity > 0.0f)
            {
                if (arpOn)
                {
                    arpAddHeld(event.note, std::clamp(event.velocity, 0.0f, 1.0f));
                }
                else
                {
                    voiceManager.noteOn(event.note, std::clamp(event.velocity, 0.0f, 1.0f),
                                        effectivePatch_, unitProfile_, lastPlayedNote);
                    lastPlayedNote = event.note;
                }
            }
            else
            {
                arpRemoveHeld(event.note);
                voiceManager.noteOff(event.note);
            }
            break;
        case Event::Type::NoteOff:
            arpRemoveHeld(event.note);
            voiceManager.noteOff(event.note);
            break;
        case Event::Type::AllNotesOff:
            arpHeldCount = 0;
            voiceManager.allNotesOff();
            break;
    }
}

void SynthEngine::processArp(int numSamples)
{
    if (effectivePatch_.arpMode <= 0)
    {
        if (arpSoundingNote >= 0)
        {
            voiceManager.noteOff(arpSoundingNote);
            arpSoundingNote = -1;
        }
        arpStepCountdown = 0.0;
        arpStepIndex = 0;
        arpDirection = 1;
        return;
    }

    // Gate: end the sounding step-note partway through the step.
    arpGateCountdown -= numSamples;
    if (arpGateCountdown <= 0.0 && arpSoundingNote >= 0)
    {
        voiceManager.noteOff(arpSoundingNote);
        arpSoundingNote = -1;
    }

    if (arpHeldCount == 0)
        return;

    arpStepCountdown -= numSamples;
    if (arpStepCountdown > 0.0)
        return;

    const double stepLength = sr / std::clamp(effectivePatch_.arpRateHz, 0.25f, 30.0f);
    arpStepCountdown += stepLength;
    if (arpStepCountdown <= 0.0) // rate went up a lot; resync
        arpStepCountdown = stepLength;

    const int octaves = std::clamp(effectivePatch_.arpOctaves, 1, 3);
    const int totalSteps = arpHeldCount * octaves;

    int step = 0;
    switch (effectivePatch_.arpMode)
    {
        case 1: // Up
            step = arpStepIndex % totalSteps;
            ++arpStepIndex;
            break;
        case 2: // Down
            step = totalSteps - 1 - (arpStepIndex % totalSteps);
            ++arpStepIndex;
            break;
        case 3: // Up-Down (ends don't repeat)
        {
            if (totalSteps == 1)
                step = 0;
            else
            {
                const int cycle = 2 * totalSteps - 2;
                const int phase = arpStepIndex % cycle;
                step = phase < totalSteps ? phase : cycle - phase;
            }
            ++arpStepIndex;
            break;
        }
        default: // Random
            step = static_cast<int>(arpRng.nextUInt64() % static_cast<std::uint64_t>(totalSteps));
            break;
    }

    const auto& held = arpHeld[step % arpHeldCount];
    const int note = held.note + 12 * (step / arpHeldCount);

    if (arpSoundingNote >= 0)
        voiceManager.noteOff(arpSoundingNote);
    voiceManager.noteOn(note, held.velocity, effectivePatch_, unitProfile_, lastPlayedNote);
    lastPlayedNote = note;
    arpSoundingNote = note;
    arpGateCountdown = stepLength * std::clamp(effectivePatch_.arpGate, 0.05f, 0.98f);
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

    // Operating mode (docs/analog-dna.md): Ideal strips every modeled
    // behavior; Exaggerated multiplies it for the educational modes.
    const auto mode = static_cast<LabMode>(labMode.load(std::memory_order_relaxed));
    float exaggerate = 1.0f;
    if (mode == LabMode::Ideal)
    {
        effectivePatch_.characterAmount = 0.0f;
        effectivePatch_.driftAmountCents = 0.0f;
        effectivePatch_.warmupAmount = 0.0f;
        effectivePatch_.mixerDrive = 1.0f;
        effectivePatch_.mixerCharacter = 0.0f;
        effectivePatch_.outputDriveDb = 0.0f;
    }
    else if (mode == LabMode::Exaggerated)
        exaggerate = 4.0f;

    sharedMod.driftFrozen = labDriftFrozen.load(std::memory_order_relaxed);
    sharedMod.characterAmount = effectivePatch_.characterAmount;

    // ---- Analog DNA correlation model -----------------------------------
    // Condition widens every tolerance; Calibration tightens tuning/cutoff
    // alignment specifically; Age drives the time-varying and dirt layers;
    // Warmth raises internal operating level into the nonlinear stages.
    {
        const PatchState& dna = effectivePatch_;
        const float master = dna.characterAmount * exaggerate;
        const float condition = 0.5f + 1.5f * dna.dnaCondition;      // Factory..Worn
        const float misalign = 1.5f - dna.dnaCalibration;            // 0.5 tight .. 1.5 loose
        sharedMod.staticScale = std::min(master * condition, 4.0f);
        sharedMod.calibScale = std::min(master * condition * misalign, 4.0f);
        sharedMod.driftScale = std::min((0.4f + 1.6f * dna.dnaAge) * exaggerate, 8.0f);
        sharedMod.noiseFloorAmp = dna.dnaAge * dna.characterAmount
                                * unitProfile_.noiseFloor * 0.0004f * exaggerate;
        sharedMod.bleedScale = dna.dnaAge * dna.characterAmount * exaggerate;
        sharedMod.warmthDrive = 0.8f + 1.4f * dna.dnaWarmth;

        // Shared circuit state: aggregate voice load sags the virtual supply
        // (fast attack ~80 ms, slow recovery ~500 ms); a stiff supply barely
        // moves. Sag pushes pitch and cutoff down and eats headroom together
        // — correlated, but with different scaling per destination.
        const float load = static_cast<float>(voiceManager.activeVoiceCount())
                         / static_cast<float>(VoiceManager::kMaxVoices);
        const float softness = std::clamp(1.0f - dna.dnaSupply + unitProfile_.supplyStiffness
                                          * dna.characterAmount, 0.0f, 1.0f);
        const float targetSag = load * softness * 0.6f;
        const float dt = static_cast<float>(numSamples / sr);
        const float coefficient = 1.0f - std::exp(-dt / (targetSag > supplySag ? 0.08f : 0.5f));
        supplySag += (targetSag - supplySag) * coefficient;
        const float sagFx = supplySag * dna.characterAmount * exaggerate;
        sharedMod.supplyPitchCents = -sagFx * 3.0f;
        sharedMod.supplyCutoffOct = -sagFx * 0.12f;
        sharedMod.supplyDrive = 1.0f + sagFx * 0.35f;

        diag.supplySag.store(supplySag, std::memory_order_relaxed);
        diag.voiceLoad.store(load, std::memory_order_relaxed);
        diag.warmupCents.store(sharedMod.warmupCents, std::memory_order_relaxed);
        diag.unitDriftNorm.store(sharedMod.unitDriftNorm, std::memory_order_relaxed);
    }

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
        processArp(chunk);

        voiceManager.forEachActiveVoice([&](SynthVoice& voice)
        {
            voice.renderChunk(left + offset, right + offset, chunk,
                              effectivePatch_, unitProfile_, sharedMod);
        });

        // Bus headroom, then the output drive into the soft knee. At 0 dB
        // drive this is exactly linear — the knee does not engage until the
        // bus is 20 dB above its nominal level. Effects + master gain run once
        // per block after all spans.
        const float d = outputDriveLinear * kBusHeadroom;
        for (int i = offset; i < offset + chunk; ++i)
        {
            left[i]  = outputStage(left[i], d);
            right[i] = outputStage(right[i], d);
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

    // Ceiling, then Master with the bus makeup folded in. The knee bounds what
    // reaches the fader at full scale; on a levelled patch the bus is 20 dB
    // below it and nothing is applied.
    smMaster.setTarget(dbToGain(patch_.masterDb));
    for (int i = 0; i < block.numSamples; ++i)
    {
        const float master = smMaster.next() * kBusMakeup;
        block.left[i] = softKnee(block.left[i]) * master;
        block.right[i] = softKnee(block.right[i]) * master;
    }

    telemetryBus.push(block.left, block.right, block.numSamples);
}

} // namespace sappsynth
