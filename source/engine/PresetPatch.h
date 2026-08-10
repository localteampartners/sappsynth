#pragma once
#include <vector>
#include "PatchState.h"
#include "../plugin/FactoryPresets.h"

namespace sappsynth {

// Framework-free bridge from the factory bank's (paramID, value) lists to a
// PatchState, so headroom/level regressions can render the real bank without
// a JUCE APVTS. The plugin still loads presets through the APVTS — this is the
// same mapping expressed once more for the core.
//
// Drift guard: `applyPresetValues` returns false on an unknown ID, and
// `preset-audit --defaults` compares `defaultPatch()` against the live APVTS
// defaults. Keep both green when a parameter is added.

// The patch a freshly-defaulted plugin produces. NOT the same as `PatchState{}`
// — several APVTS defaults differ from the struct's member initialisers, and a
// preset load resets every parameter to the APVTS default first.
PatchState defaultPatch();

// Applies one preset's value list onto `patch`. Returns false if any ID is not
// recognised (the bank and the engine have drifted apart).
bool applyPresetValues(PatchState& patch, const std::vector<presets::Value>& values);

// defaultPatch() + the preset's values.
PatchState patchForPreset(const presets::Preset& preset);

// Name of the first field where two patches differ, or nullptr if they match.
// `preset-audit --defaults` uses this to prove defaultPatch() still equals the
// patch a freshly-defaulted APVTS produces; without it a changed parameter
// default would silently make every core-side level measurement wrong.
const char* firstPatchDifference(const PatchState& a, const PatchState& b);

} // namespace sappsynth
