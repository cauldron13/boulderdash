#include "app/FixedStepRunner.h"

#include "engine/EngineTiming.h"

#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace boulderdash::app
{
namespace
{

constexpr std::uint64_t kNanosecondsPerSecond = 1000000000ULL;
constexpr std::uint64_t kC64PalSubSecondTickRateNumerator =
    static_cast<std::uint64_t>(engine::kC64PalSubSecondTicksPerCalibrationWindow) * engine::kC64PalCpuCyclesPerSecond;
constexpr std::uint64_t kC64PalSubSecondTickRateDenominator =
    engine::kC64PalCyclesPerCalibrationWindow * kNanosecondsPerSecond;
constexpr std::uint64_t kC64PalSubSecondTickRateDivisor =
    std::gcd(kC64PalSubSecondTickRateNumerator, kC64PalSubSecondTickRateDenominator);
constexpr std::uint64_t kC64PalSubSecondTickCreditsPerNanosecond =
    kC64PalSubSecondTickRateNumerator / kC64PalSubSecondTickRateDivisor;
constexpr std::uint64_t kC64PalSubSecondTickCreditThreshold =
    kC64PalSubSecondTickRateDenominator / kC64PalSubSecondTickRateDivisor;

} // namespace

FixedStepRunner::FixedStepRunner(engine::GameSession session, const std::size_t maximumLogicalFramesPerAdvance)
    : session_(std::move(session)), maximumLogicalFramesPerAdvance_(maximumLogicalFramesPerAdvance)
{
    if (maximumLogicalFramesPerAdvance_ == 0)
    {
        throw std::invalid_argument("The fixed-step runner requires at least one logical frame per advance.");
    }
}

FixedStepAdvanceResult FixedStepRunner::advance(const std::chrono::nanoseconds elapsed,
                                                const engine::GameCommand command)
{
    if (elapsed.count() < 0)
    {
        throw std::invalid_argument("The fixed-step runner cannot advance by a negative duration.");
    }
    if (paused_)
    {
        return {};
    }

    const std::uint64_t elapsedNanoseconds = static_cast<std::uint64_t>(elapsed.count());
    if (elapsedNanoseconds > (std::numeric_limits<std::uint64_t>::max() - accumulatedSubSecondTickCredits_) /
                                 kC64PalSubSecondTickCreditsPerNanosecond)
    {
        throw std::overflow_error("The fixed-step runner accumulator would overflow.");
    }
    accumulatedSubSecondTickCredits_ += elapsedNanoseconds * kC64PalSubSecondTickCreditsPerNanosecond;

    std::size_t simulatedFrameCount = 0;
    bool caveLoaded = false;
    bool presentationChanged = false;
    std::vector<engine::GameEvent> events;
    while (accumulatedSubSecondTickCredits_ >= kC64PalSubSecondTickCreditThreshold &&
           simulatedFrameCount < maximumLogicalFramesPerAdvance_)
    {
        accumulatedSubSecondTickCredits_ -= kC64PalSubSecondTickCreditThreshold;
        engine::FrameAdvanceResult frameResult = advanceOneLogicalFrame(command);
        events.insert(events.end(), frameResult.events.begin(), frameResult.events.end());
        caveLoaded = frameResult.caveLoaded;
        presentationChanged = presentationChanged || frameResult.presentationChanged;
        ++simulatedFrameCount;
        if (caveLoaded)
        {
            resetClock();
            break;
        }
    }

    return {simulatedFrameCount, accumulatedSubSecondTickCredits_ >= kC64PalSubSecondTickCreditThreshold, caveLoaded,
            presentationChanged, std::move(events)};
}

void FixedStepRunner::setPaused(const bool paused) noexcept
{
    paused_ = paused;
    if (paused_)
    {
        resetClock();
    }
}

bool FixedStepRunner::isPaused() const noexcept
{
    return paused_;
}

void FixedStepRunner::resetClock() noexcept
{
    accumulatedSubSecondTickCredits_ = 0;
}

void FixedStepRunner::reset(engine::GameSession session)
{
    session_ = std::move(session);
    resetClock();
    presentationSubSecondFrame_ = 0;
}

engine::GameSnapshot FixedStepRunner::snapshot() const
{
    return session_.snapshot();
}

engine::SessionPhase FixedStepRunner::phase() const noexcept
{
    return session_.phase();
}

std::uint64_t FixedStepRunner::presentationSubSecondFrame() const noexcept
{
    return presentationSubSecondFrame_;
}

engine::FrameAdvanceResult FixedStepRunner::advanceOneLogicalFrame(const engine::GameCommand command)
{
    ++presentationSubSecondFrame_;
    return session_.advanceFrame(command);
}

} // namespace boulderdash::app
