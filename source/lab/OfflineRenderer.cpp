#include "OfflineRenderer.h"
#include "WavWriter.h"
#include <algorithm>

namespace sappsynth {

RenderResult OfflineRenderer::render(SynthEngine& engine,
                                     const std::vector<TimedEvent>& events,
                                     double durationSeconds,
                                     int blockSize)
{
    const double sr = engine.sampleRate();
    const int totalSamples = static_cast<int>(durationSeconds * sr);

    // Convert to absolute sample positions, sorted.
    struct AbsoluteEvent { int sample; Event event; };
    std::vector<AbsoluteEvent> absolute;
    absolute.reserve(events.size());
    for (const auto& te : events)
        absolute.push_back({ static_cast<int>(te.timeSeconds * sr), te.event });
    std::stable_sort(absolute.begin(), absolute.end(),
                     [](const AbsoluteEvent& a, const AbsoluteEvent& b) { return a.sample < b.sample; });

    RenderResult result;
    result.sampleRate = sr;
    result.left.assign(static_cast<std::size_t>(totalSamples), 0.0f);
    result.right.assign(static_cast<std::size_t>(totalSamples), 0.0f);

    std::vector<Event> blockEvents;
    blockEvents.reserve(64);

    std::size_t nextEvent = 0;
    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const int n = std::min(blockSize, totalSamples - start);

        blockEvents.clear();
        while (nextEvent < absolute.size() && absolute[nextEvent].sample < start + n)
        {
            Event e = absolute[nextEvent].event;
            e.sampleOffset = std::max(0, absolute[nextEvent].sample - start);
            blockEvents.push_back(e);
            ++nextEvent;
        }

        RenderBlock block;
        block.left = result.left.data() + start;
        block.right = result.right.data() + start;
        block.numSamples = n;
        block.events = std::span<const Event>(blockEvents.data(), blockEvents.size());
        engine.process(block);
    }
    return result;
}

RenderResult OfflineRenderer::renderSingleNote(SynthEngine& engine, int note, float velocity,
                                               double noteSeconds, double totalSeconds,
                                               int blockSize)
{
    std::vector<TimedEvent> events {
        { 0.0,        { Event::Type::NoteOn,  0, note, velocity } },
        { noteSeconds, { Event::Type::NoteOff, 0, note, 0.0f } },
    };
    return render(engine, events, totalSeconds, blockSize);
}

bool OfflineRenderer::writeStereoWav(const RenderResult& result, const std::string& path)
{
    std::vector<float> interleaved(result.left.size() * 2);
    for (std::size_t i = 0; i < result.left.size(); ++i)
    {
        interleaved[2 * i] = result.left[i];
        interleaved[2 * i + 1] = result.right[i];
    }
    return writeWavFloat32(path, interleaved, 2, static_cast<int>(result.sampleRate));
}

} // namespace sappsynth
