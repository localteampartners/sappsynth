#pragma once

// Stable parameter IDs (architecture §17.1). These are project-compatibility
// surface — never derive them from UI labels, never renumber. Version suffix
// in ParameterID{id, 1} stays 1 until a migration is needed.
namespace sappsynth::param {

inline constexpr const char* osc1Wave      = "osc1.wave";
inline constexpr const char* osc1Octave    = "osc1.pitch.octave";
inline constexpr const char* osc1Semi      = "osc1.pitch.semitones";
inline constexpr const char* osc1Fine      = "osc1.pitch.cents";
inline constexpr const char* osc1Pw        = "osc1.pulseWidth";
inline constexpr const char* osc1Level     = "mixer.osc1.level";

inline constexpr const char* osc2Wave      = "osc2.wave";
inline constexpr const char* osc2Octave    = "osc2.pitch.octave";
inline constexpr const char* osc2Semi      = "osc2.pitch.semitones";
inline constexpr const char* osc2Fine      = "osc2.pitch.cents";
inline constexpr const char* osc2Pw        = "osc2.pulseWidth";
inline constexpr const char* osc2Level     = "mixer.osc2.level";
inline constexpr const char* osc2Fm        = "osc2.fmToOsc1";

inline constexpr const char* subOctave     = "sub.octave";
inline constexpr const char* subWave       = "sub.wave";
inline constexpr const char* subLevel      = "mixer.sub.level";
inline constexpr const char* noiseLevel    = "mixer.noise.level";

inline constexpr const char* mixerDrive    = "mixer.drive";
inline constexpr const char* mixerChar     = "mixer.character";

inline constexpr const char* cutoff        = "filter.cutoff.hz";
inline constexpr const char* resonance     = "filter.resonance";
inline constexpr const char* filterDrive   = "filter.drive.db";
inline constexpr const char* keyTrack      = "filter.keyTrack";
inline constexpr const char* filterEnvAmt  = "filter.envAmount";
inline constexpr const char* filterVel     = "filter.velAmount";

inline constexpr const char* ampAttack     = "env.amp.attack";
inline constexpr const char* ampDecay      = "env.amp.decay";
inline constexpr const char* ampSustain    = "env.amp.sustain";
inline constexpr const char* ampRelease    = "env.amp.release";

inline constexpr const char* filtAttack    = "env.filter.attack";
inline constexpr const char* filtDecay     = "env.filter.decay";
inline constexpr const char* filtSustain   = "env.filter.sustain";
inline constexpr const char* filtRelease   = "env.filter.release";

inline constexpr const char* lfoRate       = "lfo.rate.hz";
inline constexpr const char* lfoShape      = "lfo.shape";
inline constexpr const char* lfoToPitch    = "lfo.toPitch.cents";
inline constexpr const char* lfoToCutoff   = "lfo.toCutoff";

inline constexpr const char* polyphony     = "voice.polyphony";
inline constexpr const char* unisonCount   = "voice.unison.count";
inline constexpr const char* unisonDetune  = "voice.unison.detune.cents";
inline constexpr const char* unisonSpread  = "voice.unison.spread";
inline constexpr const char* glide         = "voice.glide.s";

inline constexpr const char* arpMode       = "arp.mode";
inline constexpr const char* arpRate       = "arp.rate.hz";
inline constexpr const char* arpOctaves    = "arp.octaves";
inline constexpr const char* arpGate       = "arp.gate";

inline constexpr const char* chorusMix     = "fx.chorus.mix";
inline constexpr const char* chorusRate    = "fx.chorus.rate.hz";
inline constexpr const char* delayTime     = "fx.delay.time.s";
inline constexpr const char* delayFeedback = "fx.delay.feedback";
inline constexpr const char* delayMix      = "fx.delay.mix";
inline constexpr const char* reverbSize    = "fx.reverb.size";
inline constexpr const char* reverbMix     = "fx.reverb.mix";

inline constexpr const char* character     = "variation.amount";
inline constexpr const char* driftAmount   = "variation.driftCents";
inline constexpr const char* driftSpeed    = "variation.driftRate";
inline constexpr const char* warmup        = "variation.warmup";

inline constexpr const char* outputDrive   = "output.drive.db";
inline constexpr const char* master        = "output.master.db";
inline constexpr const char* quality       = "quality.mode";

} // namespace sappsynth::param
