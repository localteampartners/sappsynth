// sapp-bench — CPU benchmark harness (architecture §25).
//
// Renders an 8-note chord for each quality mode and reports the realtime
// ratio (fraction of one core needed at 48 kHz / 128-sample buffers).

#include <chrono>
#include <cstdio>
#include <vector>
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"

using namespace sappsynth;

namespace {

double benchQuality(QualityMode mode, const char* name, int voices)
{
    PatchState patch;
    patch.quality = mode;
    patch.osc2.level = 0.8f;
    patch.osc2.fineCents = 7.0f;
    patch.subLevel = 0.4f;
    patch.resonance = 0.5f;
    patch.cutoffHz = 2000.0f;
    patch.filterEnvAmount = 0.4f;
    patch.polyphony = 16;

    SynthEngine engine;
    engine.setUnitSeed(42);
    engine.setPatch(patch);
    engine.prepare(48000.0, 128);

    std::vector<TimedEvent> events;
    for (int v = 0; v < voices; ++v)
        events.push_back({ 0.0, { Event::Type::NoteOn, 0, 36 + v * 5, 0.8f } });

    const double seconds = 4.0;
    const auto start = std::chrono::steady_clock::now();
    const auto result = OfflineRenderer::render(engine, events, seconds, 128);
    const auto end = std::chrono::steady_clock::now();

    const double renderTime = std::chrono::duration<double>(end - start).count();
    const double ratio = renderTime / seconds;
    std::printf("%-8s %2d voices: %6.2f%% of one core  (%.3fs for %.1fs audio)\n",
                name, voices, ratio * 100.0, renderTime, seconds);
    (void) result;
    return ratio;
}

} // namespace

int main()
{
    std::puts("SappSynth benchmark — 48 kHz, 128-sample blocks");
    benchQuality(QualityMode::Eco, "Eco", 8);
    benchQuality(QualityMode::Normal, "Normal", 16);
    benchQuality(QualityMode::High, "High", 16);
    return 0;
}
