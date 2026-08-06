// sapp-render — deterministic offline render CLI.
//
//   sapp-render out.wav [--note 48] [--velocity 100] [--seconds 2.0]
//               [--tail 1.0] [--seed 1001] [--sr 48000]
//               [--quality eco|normal|high] [--cutoff 12000] [--res 0.1]
//               [--drift 1.5] [--character 0.5] [--wave saw|pulse|sine|tri]
//
// Same engine as the plugin; fixed seed => bit-identical output.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "engine/SynthEngine.h"
#include "lab/OfflineRenderer.h"
#include "lab/SignalAnalyzer.h"

using namespace sappsynth;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::puts("usage: sapp-render out.wav [--note N] [--velocity V] [--seconds S]"
                  " [--tail T] [--seed SEED] [--sr RATE] [--quality eco|normal|high]"
                  " [--cutoff HZ] [--res R] [--drift CENTS] [--character C]"
                  " [--wave saw|pulse|sine|tri]");
        return 1;
    }

    const std::string outPath = argv[1];
    int note = 48;
    float velocity = 100.0f / 127.0f;
    double seconds = 2.0, tail = 1.0, sampleRate = 48000.0;
    std::uint64_t seed = 1001;
    PatchState patch;

    for (int i = 2; i + 1 < argc; i += 2)
    {
        const std::string key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--note") note = std::atoi(value.c_str());
        else if (key == "--velocity") velocity = static_cast<float>(std::atof(value.c_str())) / 127.0f;
        else if (key == "--seconds") seconds = std::atof(value.c_str());
        else if (key == "--tail") tail = std::atof(value.c_str());
        else if (key == "--seed") seed = static_cast<std::uint64_t>(std::atoll(value.c_str()));
        else if (key == "--sr") sampleRate = std::atof(value.c_str());
        else if (key == "--cutoff") patch.cutoffHz = static_cast<float>(std::atof(value.c_str()));
        else if (key == "--res") patch.resonance = static_cast<float>(std::atof(value.c_str()));
        else if (key == "--drift") patch.driftAmountCents = static_cast<float>(std::atof(value.c_str()));
        else if (key == "--character") patch.characterAmount = static_cast<float>(std::atof(value.c_str()));
        else if (key == "--quality")
            patch.quality = value == "eco" ? QualityMode::Eco
                          : value == "high" ? QualityMode::High : QualityMode::Normal;
        else if (key == "--wave")
        {
            patch.osc1.waveform = value == "pulse" ? Waveform::Pulse
                                : value == "sine" ? Waveform::Sine
                                : value == "tri" ? Waveform::Triangle : Waveform::Saw;
        }
        else
        {
            std::fprintf(stderr, "unknown option: %s\n", key.c_str());
            return 1;
        }
    }

    SynthEngine engine;
    engine.setUnitSeed(seed);
    engine.setPatch(patch);
    engine.prepare(sampleRate, 256);

    const auto result = OfflineRenderer::renderSingleNote(engine, note, velocity,
                                                          seconds, seconds + tail);
    if (!OfflineRenderer::writeStereoWav(result, outPath))
    {
        std::fprintf(stderr, "failed to write %s\n", outPath.c_str());
        return 1;
    }

    const int n = static_cast<int>(result.left.size());
    std::printf("wrote %s  (%.2fs @ %.0f Hz)\n", outPath.c_str(), n / sampleRate, sampleRate);
    std::printf("peak %.3f  rms %.4f  seed %llu\n",
                analyzer::peak(result.left.data(), n),
                analyzer::rms(result.left.data(), n),
                static_cast<unsigned long long>(seed));
    return 0;
}
