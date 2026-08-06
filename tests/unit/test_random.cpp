#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/variation/RandomSource.h"

using namespace sappsynth;

TEST_CASE("RandomSource is deterministic for a given seed")
{
    RandomSource a(12345), b(12345);
    for (int i = 0; i < 1000; ++i)
        REQUIRE(a.nextUInt64() == b.nextUInt64());
}

TEST_CASE("Different seeds give different streams")
{
    RandomSource a(1), b(2);
    int identical = 0;
    for (int i = 0; i < 100; ++i)
        if (a.nextUInt64() == b.nextUInt64())
            ++identical;
    REQUIRE(identical == 0);
}

TEST_CASE("nextFloat01 stays in [0,1) and covers the range")
{
    RandomSource rng(7);
    float lo = 1.0f, hi = 0.0f;
    for (int i = 0; i < 100000; ++i)
    {
        const float v = rng.nextFloat01();
        REQUIRE(v >= 0.0f);
        REQUIRE(v < 1.0f);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    REQUIRE(lo < 0.01f);
    REQUIRE(hi > 0.99f);
}

TEST_CASE("normal() has roughly zero mean and unit variance")
{
    RandomSource rng(99);
    double sum = 0.0, sumSq = 0.0;
    const int n = 200000;
    for (int i = 0; i < n; ++i)
    {
        const double v = rng.normal();
        sum += v;
        sumSq += v * v;
    }
    const double mean = sum / n;
    const double variance = sumSq / n - mean * mean;
    REQUIRE(std::abs(mean) < 0.02);
    REQUIRE(std::abs(variance - 1.0) < 0.05);
}

TEST_CASE("Seed hierarchy: sibling voice seeds are uncorrelated and stable")
{
    const std::uint64_t unit = 0xABCDEF;
    const auto v0 = seeds::voiceSeed(unit, 0);
    const auto v1 = seeds::voiceSeed(unit, 1);
    REQUIRE(v0 != v1);
    REQUIRE(v0 == seeds::voiceSeed(unit, 0)); // stable
    REQUIRE(seeds::noteSeed(v0, 0) != seeds::noteSeed(v0, 1));
    REQUIRE(seeds::noteSeed(v0, 0) != seeds::noteSeed(v1, 0));
}
