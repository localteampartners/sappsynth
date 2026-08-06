#pragma once
#include <cstdint>
#include <cmath>

namespace sappsynth {

// Deterministic random plumbing (architecture §9.5). Everything audible that
// is "random" flows from a seed hierarchy: unit -> voice -> note. Same seed,
// same render, bit for bit.

struct SplitMix64
{
    std::uint64_t state { 0 };

    std::uint64_t next() noexcept
    {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
};

class RandomSource
{
public:
    RandomSource() noexcept { seed(0x5A995A99ull); }
    explicit RandomSource(std::uint64_t seedValue) noexcept { seed(seedValue); }

    void seed(std::uint64_t seedValue) noexcept
    {
        SplitMix64 sm { seedValue };
        for (auto& word : s)
            word = sm.next();
        hasCachedNormal = false;
    }

    std::uint64_t nextUInt64() noexcept // xoshiro256**
    {
        const std::uint64_t result = rotl(s[1] * 5ull, 7) * 9ull;
        const std::uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }

    float nextFloat01() noexcept // [0, 1)
    {
        return static_cast<float>(nextUInt64() >> 40) * (1.0f / 16777216.0f);
    }

    float nextSigned() noexcept // [-1, 1)
    {
        return nextFloat01() * 2.0f - 1.0f;
    }

    float uniform(float lo, float hi) noexcept
    {
        return lo + (hi - lo) * nextFloat01();
    }

    float normal() noexcept // N(0, 1), Box-Muller with caching
    {
        if (hasCachedNormal)
        {
            hasCachedNormal = false;
            return cachedNormal;
        }
        float u1 = nextFloat01();
        if (u1 < 1.0e-7f)
            u1 = 1.0e-7f;
        const float u2 = nextFloat01();
        const float r = std::sqrt(-2.0f * std::log(u1));
        const float a = static_cast<float>(6.28318530717958647692) * u2;
        cachedNormal = r * std::sin(a);
        hasCachedNormal = true;
        return r * std::cos(a);
    }

private:
    static std::uint64_t rotl(std::uint64_t x, int k) noexcept
    {
        return (x << k) | (x >> (64 - k));
    }

    std::uint64_t s[4] {};
    float cachedNormal { 0.0f };
    bool hasCachedNormal { false };
};

// Derive child seeds from a parent seed + salt without correlation between
// siblings. unitSeed -> voiceSeed(i) -> noteSeed(counter).
namespace seeds {

inline std::uint64_t combine(std::uint64_t parent, std::uint64_t salt) noexcept
{
    SplitMix64 sm { parent ^ (salt * 0x9E3779B97F4A7C15ull + 0xD1B54A32D192ED03ull) };
    sm.next();
    return sm.next();
}

inline std::uint64_t voiceSeed(std::uint64_t unitSeed, int voiceIndex) noexcept
{
    return combine(unitSeed, 0x1000ull + static_cast<std::uint64_t>(voiceIndex));
}

inline std::uint64_t noteSeed(std::uint64_t voiceSeedValue, std::uint64_t noteCounter) noexcept
{
    return combine(voiceSeedValue, 0x2000000ull + noteCounter);
}

} // namespace seeds
} // namespace sappsynth
