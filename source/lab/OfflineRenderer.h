#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../engine/SynthEngine.h"

namespace sappsynth {

// Offline deterministic rendering (architecture §2.2/§24): the same engine the
// plugin uses, driven by a fixed event list. The foundation for regression
// tests, experiments, and the sapp-render CLI.
struct TimedEvent
{
    double timeSeconds { 0.0 };
    Event event {};
};

struct RenderResult
{
    std::vector<float> left;
    std::vector<float> right;
    double sampleRate { 48000.0 };
};

class OfflineRenderer
{
public:
    // Renders `durationSeconds` of audio in `blockSize` chunks with
    // sample-accurate event placement.
    static RenderResult render(SynthEngine& engine,
                               const std::vector<TimedEvent>& events,
                               double durationSeconds,
                               int blockSize = 256);

    // Convenience: single note held for noteSeconds inside a total render.
    static RenderResult renderSingleNote(SynthEngine& engine, int note, float velocity,
                                         double noteSeconds, double totalSeconds,
                                         int blockSize = 256);

    static bool writeStereoWav(const RenderResult& result, const std::string& path);
};

} // namespace sappsynth
