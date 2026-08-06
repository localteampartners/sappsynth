#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace sappsynth {

// Minimal 32-bit float WAV writer for offline renders and experiments.
// Not realtime code — file IO is fine here.
inline bool writeWavFloat32(const std::string& path,
                            const std::vector<float>& interleaved,
                            int numChannels, int sampleRate)
{
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
        return false;

    const std::uint32_t dataBytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(float));
    const std::uint16_t channels = static_cast<std::uint16_t>(numChannels);
    const std::uint32_t byteRate = static_cast<std::uint32_t>(sampleRate) * channels * 4u;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * 4u);

    auto u16 = [&](std::uint16_t v) { std::fwrite(&v, 2, 1, f); };
    auto u32 = [&](std::uint32_t v) { std::fwrite(&v, 4, 1, f); };

    std::fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f);
    u32(16);
    u16(3); // IEEE float
    u16(channels);
    u32(static_cast<std::uint32_t>(sampleRate));
    u32(byteRate);
    u16(blockAlign);
    u16(32);
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    std::fwrite(interleaved.data(), 1, dataBytes, f);
    std::fclose(f);
    return true;
}

} // namespace sappsynth
