#pragma once
#include <vector>

namespace sappsynth::presets {

struct Value { const char* id; float value; };
struct Preset { const char* name; const char* category; std::vector<Value> values; };

// Full factory bank, grouped by category. Values are in real parameter units;
// anything unlisted resets to default when the preset is applied.
const std::vector<Preset>& all();

} // namespace sappsynth::presets
