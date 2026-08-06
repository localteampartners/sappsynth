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
                        const PatchState& patch, const UnitProfile& unit)
{
    note_ = note;
    velocity_ = velocity;
    noteVar_ = NoteVariation::generate(noteSeedValue);
    noiseRng.seed(seeds::combine(noteSeedValue, 0x9013Eull));

    // Key-on phase: mostly free-running feel, osc2 offset so unison-style
    // patches don't phase-lock.
    osc1.noteOn(noteVar_.phase01);
    osc2.noteOn(noteVar_.phase01 + 0.37);
    sub.noteOn(0.0);

    const float character = patch.characterAmount;
    const float envTol = scaledFactor(unit.envTimeFactor, character)
                       * scaledFactor(profile_.envTimeFactor, character);
    ampEnv.setParameters(patch.ampAttack, patch.ampDecay, patch.ampSustain, patch.ampRelease);
    filterEnv.setParameters(patch.filterAttack, patch.filterDecay, patch.filterSustain, patch.filterRelease);
    const float attackVar = scaledFactor(noteVar_.attackFactor, character);
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
    const float character = mod.characterAmount;

    // ---- control-rate update -------------------------------------------
    // Correlated drift blend (architecture §9.4).
    const float vDrift = voiceDrift.process(n);
    const float o1Local = osc1Drift.process(n);
    const float o2Local = osc2Drift.process(n);
    const float driftScale = patch.driftAmountCents;
    const float drift1 = (0.35f * mod.unitDriftNorm + 0.45f * vDrift + 0.20f * o1Local) * driftScale;
    const float drift2 = (0.35f * mod.unitDriftNorm + 0.45f * vDrift + 0.20f * o2Local) * driftScale;

    const float pitchMods = mod.lfoValue * patch.lfoToPitchCents / 100.0f
                          + scaledOffset(unit.masterTuneCents + noteVar_.pitchStartCents, character) / 100.0f
                          + mod.warmupCents / 100.0f;

    osc1.setWaveform(patch.osc1.waveform);
    osc2.setWaveform(patch.osc2.waveform);
    sub.setWaveform(patch.subWaveform);
    osc1.setTolerances(scaledOffset(profile_.osc1TuneCents, character),
                       scaledOffset(profile_.osc1TrackError, character));
    osc2.setTolerances(scaledOffset(profile_.osc2TuneCents, character),
                       scaledOffset(profile_.osc2TrackError, character));
    sub.setTolerances(0.0f, 0.0f);

    osc1.updatePitch(noteWithPatchOffsets(patch.osc1) + pitchMods, drift1);
    osc2.updatePitch(noteWithPatchOffsets(patch.osc2) + pitchMods, drift2);
    // Sub tracks osc1, whole octaves down; shares osc1's drift so they beat
    // together like a divided-down circuit.
    sub.updatePitch(noteWithPatchOffsets(patch.osc1) + pitchMods - 12.0 * patch.subOctave, drift1);

    mixer.setGains(patch.osc1.level, patch.osc2.level, patch.subLevel, patch.noiseLevel);
    mixer.setCharacter(patch.mixerDrive,
                       patch.mixerCharacter * (0.25f + scaledOffset(profile_.satAsymmetry, character)));

    vca.configure(0.2f,
                  scaledOffset(profile_.satAsymmetry, character) * 0.5f,
                  dbToGain(scaledOffset(profile_.vcaGainDb + unit.outputTilt, character)));

    // Filter modifiers that hold for the whole chunk.
    const float velCut = (velocity_ - 0.5f) * 2.0f * patch.velocityToCutoff;
    const float calibrationCents = scaledOffset(unit.filterCutoffCents + profile_.filterCutoffCents, character);
    const float keyOffsetOct = patch.keyTrack * static_cast<float>(note_ - 60) / 12.0f;
    const float staticOct = keyOffsetOct + velCut + mod.lfoValue * patch.lfoToCutoff * 3.0f
                          + calibrationCents / 1200.0f;
    const float resonance = std::clamp(patch.resonance
                          + scaledOffset(unit.resonanceOffset + profile_.resonanceOffset, character),
                          0.0f, 1.1f);
    const float filterDrive = dbToGain(patch.filterDriveDb);

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

        const float o1 = osc1.tick(pwm);
        const float o2 = osc2.tick(pwm2);
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
        return vca.process(x, ampEnvBuf[static_cast<std::size_t>(baseIndex)]);
    });

    // ---- pan + accumulate ----------------------------------------------
    const float pan = std::clamp(scaledOffset(profile_.panBias, character), -1.0f, 1.0f);
    // +8 dB voice makeup: recovers the mixer's 0.5 reference staging and the
    // VCA curve so a single full-level oscillator lands near -10 dBFS.
    constexpr float kVoiceMakeup = 2.5f;
    const float panL = kVoiceMakeup * std::cos((pan + 1.0f) * static_cast<float>(kPi) * 0.25f);
    const float panR = kVoiceMakeup * std::sin((pan + 1.0f) * static_cast<float>(kPi) * 0.25f);
    for (int i = 0; i < n; ++i)
    {
        const float y = signalBuf[static_cast<std::size_t>(i)];
        left[i] += y * panL;
        right[i] += y * panR;
    }
}

} // namespace sappsynth
