#pragma once
#include <array>

// SappLink v1 CC-in mapping (framework-free so tests can link it).
// Source of truth: ~/apps/sapptune/sapplink/manifests/sappsynth.json — the
// unit test asserts this table matches the vendored copy in tests/data/.
// CC 1 (mod wheel), CC 64 (sustain) and pitch bend are deliberately absent.
namespace sappsynth::sapplink {

enum class Curve { Linear, Log };

struct CCMapping
{
    int cc;
    const char* paramId; // stable dotted ID from ParameterIds.h, verbatim
    float lo, hi;        // engineering units at CC 0 and CC 127
    Curve curve;
};

constexpr int kNumMappings = 20;
const std::array<CCMapping, kNumMappings>& mappings();

// nullptr if this CC is not part of the SappLink contract.
const CCMapping* findMapping(int cc);

// CC value 0..127 -> engineering units through the mapping's curve.
float ccToEngineering(const CCMapping& mapping, int ccValue);

} // namespace sappsynth::sapplink
