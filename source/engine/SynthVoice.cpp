#include "SynthVoice.h"
#include <cmath>

namespace sappsynth {

namespace {
// Character amount scales tolerances: additive offsets scale linearly, time
// factors scale toward 1.
inline float scaledOffset(float value, float character) noexcept { return value * character; }
inline float scaledFactor(float factor, float character) noexcept { return 1.0f + (factor - 1.0f) * character; }
} // namespace

void SynthVoice::prepare(double sampleRate)
{
    sr = sampleRate;
    osc1.prepare(sr);
    osc2.prepare(sr);
    sub.prepare(sr);
    ampEnv.prepare(sr);
    filterEnv.prepare(sr);
    oversampler.prepare();
    filter.prepare(sr * oversampleFactor);
    dcBlocker.prepare(sr * oversampleFactor);
    voiceDrift.prepare(sr);
    osc1Drift.prepare(sr);
    osc2Drift.prepare(sr);
    note_ = -1;
    ampEnv.reset();
    filterEnv.reset();
}

void SynthVoice::configureIdentity(int voiceIndex, std::uint64_t unitSeed)
{
    index_ = voiceIndex;
    voiceSeed_ = seeds::voiceSeed(unitSeed, voiceIndex);
    profile_ = VoiceProfile::generate(voiceSeed_);
    voiceDrift.seed(seeds::combine(voiceSeed_, 0xD41Full));
    osc1Drift.seed(seeds::combine(voiceSeed_, 0xD41F1ull));
    osc2Drift.seed(seeds::combine(voiceSeed_, 0xD41F2ull));
}

void SynthVoice::applyQuality(const ProcessingQuality& quality)
{
    oversampleFactor = quality.oversamplingFactor;
    oversampler.setFactor(oversampleFactor);
    oversampler.reset();
    filter.prepare(sr * oversampleFactor);
    filter.setSolverIterations(quality.filterSolverIterations);
    dcBlocker.prepare(sr * oversampleFactor);
    const auto method = quality.naiveOscillators ? OscillatorMethod::Naive
                                                 : OscillatorMethod::PolyBlep;
    osc1.setMethod(method);
    osc2.setMethod(method);
    sub.setMethod(method);
}

void SynthVoice::noteOn(int note, float velocity, std::uint64_t noteSeedValue,
                        const PatchState& patch, const UnitProfile& unit,
                        const NoteStart& start)
{
    note_ = note;
    velocity_ = velocity;
    start_ = start;
    glideOffsetSemis = (patch.glideSeconds > 0.001f && start.glideFromNote >= 0)
                     ? static_cast<float>(start.glideFromNote - note)
                     : 0.0f;
    noteVar_ = NoteVariation::generate(noteSeedValue);
    noiseRng.seed(seeds::combine(noteSeedValue, 0x9013Eull));

    // Key-on phase: mostly free-running feel, osc2 offset so unison-style
    // patches don't phase-lock.
    osc1.noteOn(noteVar_.phase01);
    osc2.noteOn(noteVar_.phase01 + 0.37);
    sub.noteOn(0.0);

    // Note-on tolerances use the DNA static scale (master x condition).
    const float character = patch.characterAmount * (0.5f + 1.5f * patch.dnaCondition);
    const float envTol = scaledFactor(unit.envTimeFactor, character)
                       * scaledFactor(profile_.envTimeFactor, character);
    const float sustainOffset = scaledOffset(profile_.envSustainOffset, character);
    ampEnv.setParameters(patch.ampAttack, patch.ampDecay,
                         std::clamp(patch.ampSustain + sustainOffset, 0.0f, 1.0f), patch.ampRelease);
    filterEnv.setParameters(patch.filterAttack, patch.filterDecay,
                            std::clamp(patch.filterSustain + sustainOffset, 0.0f, 1.0f), patch.filterRelease);
    const float attackVar = scaledFactor(noteVar_.attackFactor, character)
                          * scaledFactor(profile_.envAttackScale, character);
    ampEnv.noteOn(envTol, attackVar);
    filterEnv.noteOn(envTol, attackVar);
}

void SynthVoice::noteOff()
{
    ampEnv.noteOff();
    filterEnv.noteOff();
}

void SynthVoice::steal()
{
    ampEnv.fastRelease();
    filterEnv.fastRelease();
}

void SynthVoice::hardStop()
{
    ampEnv.reset();
    filterEnv.reset();
    filter.reset();
    dcBlocker.reset();
    oversampler.reset();
    note_ = -1;
}

double SynthVoice::noteWithPatchOffsets(const OscillatorParams& p) const noexcept
{
    return static_cast<double>(note_)
         + 12.0 * p.octave
         + static_cast<double>(p.semitones)
         + static_cast<double>(p.fineCents) / 100.0;
}

void SynthVoice::renderChunk(float* left, float* right, int numSamples,
                             const PatchState& patch, const UnitProfile& unit,
                             const SharedModulation& mod)
{
    if (!ampEnv.isActive() || numSamples <= 0)
        return;

    const int n = std::min(numSamples, kControlInterval);
    // DNA grouped scales: calibScale for tuning/cutoff alignment, staticScale
    // for everything else static (see docs/analog-dna.md correlation model).
    const float character = mod.staticScale;
    const float calib = mod.calibScale;

    // ---- control-rate update -------------------------------------------
    // Correlated drift blend (architecture §9.4). Frozen in Lab mode: hold
    // the current values so the listener can isolate what drift adds.
    const float vDrift = mod.driftFrozen ? voiceDrift.value() : voiceDrift.process(n);
    const float o1Local = mod.driftFrozen ? osc1Drift.value() : osc1Drift.process(n);
    const float o2Local = mod.driftFrozen ? osc2Drift.value() : osc2Drift.process(n);
    const float driftScale = patch.driftAmountCents * mod.driftScale;
    const float drift1 = (0.35f * mod.unitDriftNorm + 0.45f * vDrift + 0.20f * o1Local) * driftScale;
    const float drift2 = (0.35f * mod.unitDriftNorm + 0.45f * vDrift + 0.20f * o2Local) * driftScale;

    // Glide: exponential approach from the previous note in semitone space.
    if (glideOffsetSemis != 0.0f)
    {
        const float coef = std::exp(-static_cast<float>(n)
                                    / (std::max(patch.glideSeconds, 0.005f) * static_cast<float>(sr) * 0.3f));
        glideOffsetSemis *= coef;
        if (std::abs(glideOffsetSemis) < 0.001f)
            glideOffsetSemis = 0.0f;
    }

    const float pitchMods = mod.lfoValue * patch.lfoToPitchCents / 100.0f
                          + scaledOffset(unit.masterTuneCents, calib) / 100.0f
                          + scaledOffset(noteVar_.pitchStartCents, character) / 100.0f
                          + (mod.warmupCents + mod.supplyPitchCents) / 100.0f
                          + start_.unisonDetuneCents / 100.0f
                          + glideOffsetSemis;

    osc1.setWaveform(patch.osc1.waveform);
    osc2.setWaveform(patch.osc2.waveform);
    sub.setWaveform(patch.subWaveform);
    osc1.setTolerances(scaledOffset(profile_.osc1TuneCents, calib),
                       scaledOffset(profile_.osc1TrackError, calib));
    osc2.setTolerances(scaledOffset(profile_.osc2TuneCents, calib),
                       scaledOffset(profile_.osc2TrackError, calib));
    sub.setTolerances(0.0f, 0.0f);

    osc1.updatePitch(noteWithPatchOffsets(patch.osc1) + pitchMods, drift1);
    osc2.updatePitch(noteWithPatchOffsets(patch.osc2) + pitchMods, drift2);
    // Sub tracks osc1, whole octaves down; shares osc1's drift so they beat
    // together like a divided-down circuit.
    sub.updatePitch(noteWithPatchOffsets(patch.osc1) + pitchMods - 12.0 * patch.subOctave, drift1);

    mixer.setGains(patch.osc1.level, patch.osc2.level, patch.subLevel, patch.noiseLevel);
    // Warmth raises the internal operating level into the mixer saturator;
    // supply sag eats headroom the same way (correlated, §shared circuit).
    mixer.setCharacter(patch.mixerDrive * mod.warmthDrive * mod.supplyDrive,
                       patch.mixerCharacter * (0.25f + scaledOffset(profile_.satAsymmetry, character)));

    vca.configure(0.2f,
                  scaledOffset(profile_.satAsymmetry, character) * 0.5f,
                  dbToGain(scaledOffset(profile_.vcaGainDb + unit.outputTilt, character)));

    // Filter modifiers that hold for the whole chunk.
    const float velCut = (velocity_ - 0.5f) * 2.0f * patch.velocityToCutoff;
    const float calibrationCents = scaledOffset(unit.filterCutoffCents + profile_.filterCutoffCents, calib);
    const float keyOffsetOct = patch.keyTrack * static_cast<float>(note_ - 60) / 12.0f;
    const float staticOct = keyOffsetOct + velCut + mod.lfoValue * patch.lfoToCutoff * 3.0f
                          + calibrationCents / 1200.0f + mod.supplyCutoffOct;
    const float resonance = std::clamp(patch.resonance
                          + scaledOffset(unit.resonanceOffset + profile_.resonanceOffset, character),
                          0.0f, 1.1f);
    const float filterDrive = dbToGain(patch.filterDriveDb) * mod.supplyDrive;
    const float bleedAmp = profile_.vcaBleed * mod.bleedScale * 0.006f;

    const float pwm = patch.osc1.pulseWidth + scaledOffset(profile_.pulseWidthOffset, character);
    const float pwm2 = patch.osc2.pulseWidth + scaledOffset(profile_.pulseWidthOffset, character);

    // Velocity into amplitude with a soft curve plus per-note tilt.
    const float velGain = (0.25f + 0.75f * velocity_ * velocity_)
                        * (1.0f + scaledOffset(noteVar_.velocityTilt, character));

    // ---- base-rate render ----------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        ampEnvBuf[static_cast<std::size_t>(i)] = ampEnv.tick() * velGain;
        const float fEnv = filterEnv.tick();
        const float totalOct = staticOct + patch.filterEnvAmount * 5.0f * fEnv;
        cutoffBuf[static_cast<std::size_t>(i)] = patch.cutoffHz * std::exp2(totalOct);

        // Osc2 renders first so it can frequency-modulate osc1 this sample.
        const float o2 = osc2.tick(pwm2);
        const float fmMult = 1.0f + patch.osc2ToOsc1Fm * 4.0f * o2;
        const float o1 = osc1.tick(pwm, fmMult);
        const float sb = sub.tick(0.5f);
        const float nz = noiseRng.nextSigned();
        signalBuf[static_cast<std::size_t>(i)] = mixer.weightedSum(o1, o2, sb, nz);
    }

    // ---- oversampled nonlinear island: mixer sat -> ladder -> VCA -------
    // tan() only at chunk edges; G interpolates across the chunk (32 samples
    // of a smoothed cutoff — the curvature error is inaudible, the CPU is not).
    filter.setTone(resonance, filterDrive);
    const float gStart = filter.computeG(cutoffBuf[0]);
    const float gEnd = filter.computeG(cutoffBuf[static_cast<std::size_t>(n - 1)]);
    const float gStep = n > 1 ? (gEnd - gStart) / static_cast<float>(n - 1) : 0.0f;
    int lastBase = -1;
    oversampler.process(signalBuf.data(), n, [&](float x, int baseIndex) noexcept
    {
        if (baseIndex != lastBase)
        {
            filter.setG(gStart + gStep * static_cast<float>(baseIndex));
            lastBase = baseIndex;
        }
        x = dcBlocker.process(mixer.saturateSum(x));
        x = filter.process(x);
        return vca.process(x, ampEnvBuf[static_cast<std::size_t>(baseIndex)], bleedAmp);
    });

    // ---- pan + accumulate ----------------------------------------------
    const float pan = std::clamp(scaledOffset(profile_.panBias, character) + start_.unisonPan,
                                 -1.0f, 1.0f);
    // +8 dB voice makeup: recovers the mixer's 0.5 reference staging and the
    // VCA curve so a single full-level oscillator lands near -10 dBFS.
    constexpr float kVoiceMakeup = 2.5f;
    const float gain = kVoiceMakeup * start_.unisonGain;
    const float panL = gain * std::cos((pan + 1.0f) * static_cast<float>(kPi) * 0.25f);
    const float panR = gain * std::sin((pan + 1.0f) * static_cast<float>(kPi) * 0.25f);
    for (int i = 0; i < n; ++i)
    {
        // Unit noise floor: this instrument's hiss, present while the voice
        // sounds (Age x DNA amount x per-unit factor).
        const float y = signalBuf[static_cast<std::size_t>(i)]
                      + mod.noiseFloorAmp * noiseRng.nextSigned();
        left[i] += y * panL;
        right[i] += y * panR;
    }
}

} // namespace sappsynth
