#include "SappLinkCCMap.h"
#include <algorithm>
#include <cmath>
#include "../parameters/ParameterIds.h"

namespace sappsynth::sapplink {
namespace p = sappsynth::param;

// Ranges are the plugin's REAL parameter ranges (createLayout), not the
// 2026-08-06 manifest's — see docs/sapplink.md for the 8 corrections the
// manifest needs. Diverging silently would pin CC ranges against the
// parameter clamp; agreeing loudly is the contract.
const std::array<CCMapping, kNumMappings>& mappings()
{
    static const std::array<CCMapping, kNumMappings> table { {
        { 74, p::cutoff,        20.0f,    20000.0f, Curve::Log },
        { 71, p::resonance,     0.0f,     1.0f,     Curve::Linear },
        { 24, p::filterEnvAmt,  -1.0f,    1.0f,     Curve::Linear },
        { 73, p::ampAttack,     0.001f,   5.0f,     Curve::Log },
        { 72, p::ampRelease,    0.001f,   5.0f,     Curve::Log },
        { 5,  p::glide,         0.0f,     1.0f,     Curve::Linear },
        { 76, p::lfoRate,       0.02f,    20.0f,    Curve::Log },
        { 77, p::lfoToCutoff,   0.0f,     1.0f,     Curve::Linear },
        { 78, p::lfoToPitch,    0.0f,     100.0f,   Curve::Linear },
        { 25, p::mixerDrive,    1.0f,     8.0f,     Curve::Log },
        { 93, p::chorusMix,     0.0f,     1.0f,     Curve::Linear },
        { 94, p::delayMix,      0.0f,     1.0f,     Curve::Linear },
        { 95, p::delayFeedback, 0.0f,     0.85f,    Curve::Linear },
        { 91, p::reverbMix,     0.0f,     1.0f,     Curve::Linear },
        { 92, p::reverbSize,    0.0f,     1.0f,     Curve::Linear },
        { 26, p::driftAmount,   0.0f,     10.0f,    Curve::Linear },
        { 27, p::driftSpeed,    0.0f,     1.0f,     Curve::Linear },
        { 28, p::arpRate,       0.5f,     20.0f,    Curve::Log },
        { 29, p::arpGate,       0.05f,    0.95f,    Curve::Linear },
        { 7,  p::master,        -40.0f,   6.0f,     Curve::Linear },
    } };
    return table;
}

const CCMapping* findMapping(int cc)
{
    for (const auto& mapping : mappings())
        if (mapping.cc == cc)
            return &mapping;
    return nullptr;
}

float ccToEngineering(const CCMapping& mapping, int ccValue)
{
    const float t = static_cast<float>(std::clamp(ccValue, 0, 127)) / 127.0f;
    if (mapping.curve == Curve::Log)
        return mapping.lo * std::pow(mapping.hi / mapping.lo, t);
    return mapping.lo + (mapping.hi - mapping.lo) * t;
}

} // namespace sappsynth::sapplink
