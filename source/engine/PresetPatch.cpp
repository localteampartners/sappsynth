#include "PresetPatch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../parameters/ParameterIds.h"

namespace sappsynth {
namespace p = param;

PatchState defaultPatch()
{
    PatchState patch;
    // Mirrors createParameterLayout() in PluginProcessor.cpp. Only the fields
    // whose APVTS default differs from the struct initialiser need listing;
    // they are listed anyway where the value is load-bearing for levels.
    patch.mixerDrive = 1.0f;
    patch.mixerCharacter = 0.15f;
    patch.cutoffHz = 9000.0f;
    patch.filterEnvAmount = 0.35f;
    return patch;
}

bool applyPresetValues(PatchState& patch, const std::vector<presets::Value>& values)
{
    auto same = [](const char* a, const char* b) { return std::strcmp(a, b) == 0; };

    for (const auto& [id, v] : values)
    {
        const int index = static_cast<int>(v);

        if      (same(id, p::osc1Wave))    patch.osc1.waveform = static_cast<Waveform>(index);
        else if (same(id, p::osc1Octave))  patch.osc1.octave = index;
        else if (same(id, p::osc1Semi))    patch.osc1.semitones = index;
        else if (same(id, p::osc1Fine))    patch.osc1.fineCents = v;
        else if (same(id, p::osc1Pw))      patch.osc1.pulseWidth = v;
        else if (same(id, p::osc1Level))   patch.osc1.level = v;

        else if (same(id, p::osc2Wave))    patch.osc2.waveform = static_cast<Waveform>(index);
        else if (same(id, p::osc2Octave))  patch.osc2.octave = index;
        else if (same(id, p::osc2Semi))    patch.osc2.semitones = index;
        else if (same(id, p::osc2Fine))    patch.osc2.fineCents = v;
        else if (same(id, p::osc2Pw))      patch.osc2.pulseWidth = v;
        else if (same(id, p::osc2Level))   patch.osc2.level = v;
        else if (same(id, p::osc2Fm))      patch.osc2ToOsc1Fm = v;

        // Choice 0/1 on the panel means one/two octaves down.
        else if (same(id, p::subOctave))   patch.subOctave = index + 1;
        else if (same(id, p::subWave))     patch.subWaveform = index == 0 ? Waveform::Pulse : Waveform::Sine;
        else if (same(id, p::subLevel))    patch.subLevel = v;
        else if (same(id, p::noiseLevel))  patch.noiseLevel = v;

        else if (same(id, p::mixerDrive))  patch.mixerDrive = v;
        else if (same(id, p::mixerChar))   patch.mixerCharacter = v;

        else if (same(id, p::cutoff))      patch.cutoffHz = v;
        else if (same(id, p::resonance))   patch.resonance = v;
        else if (same(id, p::filterDrive)) patch.filterDriveDb = v;
        else if (same(id, p::keyTrack))    patch.keyTrack = v;
        else if (same(id, p::filterEnvAmt))patch.filterEnvAmount = v;
        else if (same(id, p::filterVel))   patch.velocityToCutoff = v;

        else if (same(id, p::ampAttack))   patch.ampAttack = v;
        else if (same(id, p::ampDecay))    patch.ampDecay = v;
        else if (same(id, p::ampSustain))  patch.ampSustain = v;
        else if (same(id, p::ampRelease))  patch.ampRelease = v;
        else if (same(id, p::filtAttack))  patch.filterAttack = v;
        else if (same(id, p::filtDecay))   patch.filterDecay = v;
        else if (same(id, p::filtSustain)) patch.filterSustain = v;
        else if (same(id, p::filtRelease)) patch.filterRelease = v;

        else if (same(id, p::lfoRate))     patch.lfoRateHz = v;
        else if (same(id, p::lfoShape))    patch.lfoShape = static_cast<LfoShape>(index);
        else if (same(id, p::lfoToPitch))  patch.lfoToPitchCents = v;
        else if (same(id, p::lfoToCutoff)) patch.lfoToCutoff = v;

        else if (same(id, p::polyphony))   patch.polyphony = index;
        else if (same(id, p::unisonCount)) patch.unisonCount = index;
        else if (same(id, p::unisonDetune))patch.unisonDetuneCents = v;
        else if (same(id, p::unisonSpread))patch.unisonSpread = v;
        else if (same(id, p::glide))       patch.glideSeconds = v;

        else if (same(id, p::arpMode))     patch.arpMode = index;
        else if (same(id, p::arpRate))     patch.arpRateHz = v;
        else if (same(id, p::arpOctaves))  patch.arpOctaves = index;
        else if (same(id, p::arpGate))     patch.arpGate = v;

        else if (same(id, p::chorusMix))   patch.chorusMix = v;
        else if (same(id, p::chorusRate))  patch.chorusRateHz = v;
        else if (same(id, p::delayTime))   patch.delayTimeS = v;
        else if (same(id, p::delayFeedback)) patch.delayFeedback = v;
        else if (same(id, p::delayMix))    patch.delayMix = v;
        else if (same(id, p::reverbSize))  patch.reverbSize = v;
        else if (same(id, p::reverbMix))   patch.reverbMix = v;

        else if (same(id, p::character))   patch.characterAmount = v;
        else if (same(id, p::dnaCondition))patch.dnaCondition = v;
        else if (same(id, p::dnaCalibration)) patch.dnaCalibration = v;
        else if (same(id, p::dnaWarmth))   patch.dnaWarmth = v;
        else if (same(id, p::dnaSupply))   patch.dnaSupply = v;
        else if (same(id, p::dnaAge))      patch.dnaAge = v;
        else if (same(id, p::driftAmount)) patch.driftAmountCents = v;
        else if (same(id, p::driftSpeed))  patch.driftSpeed = v;
        else if (same(id, p::warmup))      patch.warmupAmount = v;

        else if (same(id, p::outputDrive)) patch.outputDriveDb = v;
        else if (same(id, p::master))      patch.masterDb = v;
        else if (same(id, p::quality))     patch.quality = static_cast<QualityMode>(index);

        else return false;
    }
    return true;
}

PatchState patchForPreset(const presets::Preset& preset)
{
    PatchState patch = defaultPatch();
    applyPresetValues(patch, preset.values);
    return patch;
}

namespace {
// A parameter's default survives a 0..1 normalise/denormalise round trip in the
// APVTS, which lands 0.0 on 7.5e-07. Compare floats with a tolerance scaled to
// the value: tight enough to catch any default anyone would actually change,
// loose enough to ignore float noise.
bool nearlyEqual(float a, float b) noexcept
{
    const float scale = std::max({ 1.0f, std::abs(a), std::abs(b) });
    return std::abs(a - b) <= 1.0e-4f * scale;
}
} // namespace

const char* firstPatchDifference(const PatchState& a, const PatchState& b)
{
#define SAPP_DIFF_F(field) if (!nearlyEqual(a.field, b.field)) return #field;
#define SAPP_DIFF_X(field) if (!(a.field == b.field)) return #field;
    SAPP_DIFF_X(osc1.waveform) SAPP_DIFF_X(osc1.octave) SAPP_DIFF_X(osc1.semitones)
    SAPP_DIFF_F(osc1.fineCents) SAPP_DIFF_F(osc1.pulseWidth) SAPP_DIFF_F(osc1.level)
    SAPP_DIFF_X(osc2.waveform) SAPP_DIFF_X(osc2.octave) SAPP_DIFF_X(osc2.semitones)
    SAPP_DIFF_F(osc2.fineCents) SAPP_DIFF_F(osc2.pulseWidth) SAPP_DIFF_F(osc2.level)
    SAPP_DIFF_F(osc2ToOsc1Fm)
    SAPP_DIFF_X(subOctave) SAPP_DIFF_X(subWaveform) SAPP_DIFF_F(subLevel) SAPP_DIFF_F(noiseLevel)
    SAPP_DIFF_F(mixerDrive) SAPP_DIFF_F(mixerCharacter)
    SAPP_DIFF_F(cutoffHz) SAPP_DIFF_F(resonance) SAPP_DIFF_F(filterDriveDb)
    SAPP_DIFF_F(keyTrack) SAPP_DIFF_F(filterEnvAmount) SAPP_DIFF_F(velocityToCutoff)
    SAPP_DIFF_F(ampAttack) SAPP_DIFF_F(ampDecay) SAPP_DIFF_F(ampSustain) SAPP_DIFF_F(ampRelease)
    SAPP_DIFF_F(filterAttack) SAPP_DIFF_F(filterDecay)
    SAPP_DIFF_F(filterSustain) SAPP_DIFF_F(filterRelease)
    SAPP_DIFF_F(lfoRateHz) SAPP_DIFF_X(lfoShape)
    SAPP_DIFF_F(lfoToPitchCents) SAPP_DIFF_F(lfoToCutoff)
    SAPP_DIFF_X(polyphony)
    SAPP_DIFF_X(arpMode) SAPP_DIFF_F(arpRateHz) SAPP_DIFF_X(arpOctaves) SAPP_DIFF_F(arpGate)
    SAPP_DIFF_X(unisonCount) SAPP_DIFF_F(unisonDetuneCents)
    SAPP_DIFF_F(unisonSpread) SAPP_DIFF_F(glideSeconds)
    SAPP_DIFF_F(chorusMix) SAPP_DIFF_F(chorusRateHz)
    SAPP_DIFF_F(delayTimeS) SAPP_DIFF_F(delayFeedback) SAPP_DIFF_F(delayMix)
    SAPP_DIFF_F(reverbSize) SAPP_DIFF_F(reverbMix)
    SAPP_DIFF_F(characterAmount) SAPP_DIFF_F(driftAmountCents)
    SAPP_DIFF_F(driftSpeed) SAPP_DIFF_F(warmupAmount)
    SAPP_DIFF_F(dnaCondition) SAPP_DIFF_F(dnaCalibration) SAPP_DIFF_F(dnaWarmth)
    SAPP_DIFF_F(dnaSupply) SAPP_DIFF_F(dnaAge)
    SAPP_DIFF_F(outputDriveDb) SAPP_DIFF_F(masterDb) SAPP_DIFF_X(quality)
#undef SAPP_DIFF_F
#undef SAPP_DIFF_X
    return nullptr;
}

} // namespace sappsynth
