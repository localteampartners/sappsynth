#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "dsp/modulation/Envelope.h"

using namespace sappsynth;

TEST_CASE("Attack reaches full level in the requested time")
{
    const double sr = 48000.0;
    Envelope env;
    env.prepare(sr);
    env.setParameters(0.1f, 0.2f, 0.5f, 0.3f);
    env.noteOn(1.0f, 1.0f);

    const int attackSamples = static_cast<int>(0.1 * sr);
    int reached = -1;
    for (int i = 0; i < attackSamples * 2; ++i)
    {
        if (env.tick() >= 0.999f && reached < 0)
        {
            reached = i;
            break;
        }
    }
    REQUIRE(reached > 0);
    // Within 15% of the requested attack time.
    REQUIRE(std::abs(reached - attackSamples) < attackSamples * 0.15);
}

TEST_CASE("Decay settles to sustain and release reaches silence")
{
    const double sr = 48000.0;
    Envelope env;
    env.prepare(sr);
    env.setParameters(0.001f, 0.05f, 0.6f, 0.05f);
    env.noteOn(1.0f, 1.0f);

    for (int i = 0; i < static_cast<int>(0.5 * sr); ++i)
        env.tick();
    REQUIRE(std::abs(env.value() - 0.6f) < 0.01f);
    REQUIRE(env.currentStage() == Envelope::Stage::Sustain);

    env.noteOff();
    for (int i = 0; i < static_cast<int>(0.5 * sr); ++i)
        env.tick();
    REQUIRE(env.value() == 0.0f);
    REQUIRE(!env.isActive());
}

TEST_CASE("Retrigger continues from the current level (no click to zero)")
{
    const double sr = 48000.0;
    Envelope env;
    env.prepare(sr);
    env.setParameters(0.05f, 0.1f, 0.8f, 0.2f);
    env.noteOn(1.0f, 1.0f);
    for (int i = 0; i < static_cast<int>(0.2 * sr); ++i)
        env.tick();
    const float levelBefore = env.value();
    REQUIRE(levelBefore > 0.5f);

    env.noteOn(1.0f, 1.0f); // retrigger mid-flight
    const float levelAfter = env.tick();
    REQUIRE(levelAfter >= levelBefore * 0.99f); // no reset to zero
}

TEST_CASE("Timing tolerance scales segment length")
{
    const double sr = 48000.0;
    auto timeToFull = [&](float tolerance)
    {
        Envelope env;
        env.prepare(sr);
        env.setParameters(0.05f, 0.1f, 0.8f, 0.2f);
        env.noteOn(tolerance, 1.0f);
        for (int i = 0; i < static_cast<int>(sr); ++i)
            if (env.tick() >= 0.999f)
                return i;
        return -1;
    };

    const int nominal = timeToFull(1.0f);
    const int slower = timeToFull(1.2f);
    REQUIRE(nominal > 0);
    REQUIRE(slower > nominal);
}

TEST_CASE("fastRelease reaches silence within ~5 ms")
{
    const double sr = 48000.0;
    Envelope env;
    env.prepare(sr);
    env.setParameters(0.001f, 0.1f, 1.0f, 2.0f);
    env.noteOn(1.0f, 1.0f);
    for (int i = 0; i < 4800; ++i)
        env.tick();
    env.fastRelease();
    for (int i = 0; i < static_cast<int>(0.005 * sr); ++i)
        env.tick();
    REQUIRE(env.value() < 0.02f);
}
