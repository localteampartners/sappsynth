#pragma once
#include <algorithm>
#include <cmath>
#include "../utility/FastMath.h"

namespace sappsynth {

// ADSR with one-pole target-based exponential segments (architecture §13.2).
// Attack aims *past* 1.0 so it actually arrives in the requested time instead
// of asymptotically crawling. Per-voice timing tolerance scales all segments.
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare(double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        level = 0.0f;
    }

    void setParameters(float attackSeconds, float decaySeconds,
                       float sustainLevel, float releaseSeconds) noexcept
    {
        attackTime  = std::max(attackSeconds, 0.0005f);
        decayTime   = std::max(decaySeconds, 0.001f);
        sustain     = std::clamp(sustainLevel, 0.0f, 1.0f);
        releaseTime = std::max(releaseSeconds, 0.002f);
    }

    // toleranceFactor: per-voice/unit envelope timing bias (~1.0).
    // attackFactor: per-note variation (~1.0). Retrigger keeps current level.
    void noteOn(float toleranceFactor, float attackFactor) noexcept
    {
        timeScale = std::max(toleranceFactor, 0.05f);
        // Attack coefficient: reach 1.0 (target 1.3) in attackTime.
        attackCoef  = coefficientForRatio(attackTime * timeScale * attackFactor, 0.3f / 1.3f);
        decayCoef   = coefficientForRatio(decayTime * timeScale, 0.01f);
        releaseCoef = coefficientForRatio(releaseTime * timeScale, 0.01f);
        stage = Stage::Attack;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    // De-click ramp used when a voice is stolen: ~3 ms to silence.
    void fastRelease() noexcept
    {
        releaseCoef = coefficientForRatio(0.003f, 0.01f);
        stage = Stage::Release;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    Stage currentStage() const noexcept { return stage; }

    float tick() noexcept
    {
        switch (stage)
        {
            case Stage::Idle:
                return 0.0f;

            case Stage::Attack:
                level = kAttackTarget + attackCoef * (level - kAttackTarget);
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;

            case Stage::Decay:
                level = sustain + decayCoef * (level - sustain);
                if (level - sustain < 1.0e-4f)
                    stage = Stage::Sustain;
                break;

            case Stage::Sustain:
                level = sustain;
                break;

            case Stage::Release:
                level = releaseCoef * level;
                if (level < 1.0e-5f)
                {
                    level = 0.0f;
                    stage = Stage::Idle;
                }
                break;
        }
        return level;
    }

    float value() const noexcept { return level; }

private:
    // Coefficient so that a segment covers all but `residualRatio` of its
    // distance in `seconds`.
    float coefficientForRatio(float seconds, float residualRatio) const noexcept
    {
        const double samples = std::max(1.0, static_cast<double>(seconds) * sr);
        return static_cast<float>(std::exp(std::log(static_cast<double>(residualRatio)) / samples));
    }

    static constexpr float kAttackTarget = 1.3f;

    double sr { 48000.0 };
    Stage stage { Stage::Idle };
    float level { 0.0f };
    float attackTime { 0.01f }, decayTime { 0.1f }, sustain { 0.8f }, releaseTime { 0.2f };
    float attackCoef { 0.99f }, decayCoef { 0.999f }, releaseCoef { 0.999f };
    float timeScale { 1.0f };
};

} // namespace sappsynth
