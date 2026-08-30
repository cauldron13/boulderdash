#pragma once

#include "engine/GameSession.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace boulderdash::app
{

struct FixedStepAdvanceResult final
{
    std::size_t simulatedFrameCount = 0;
    bool hasBacklog = false;
    bool caveLoaded = false;
    bool presentationChanged = false;
    std::vector<engine::GameEvent> events;
};

class FixedStepRunner final
{
  public:
    explicit FixedStepRunner(engine::GameSession session, std::size_t maximumLogicalFramesPerAdvance = 8);

    [[nodiscard]] FixedStepAdvanceResult advance(std::chrono::nanoseconds elapsed, engine::GameCommand command);
    void setPaused(bool paused) noexcept;
    [[nodiscard]] bool isPaused() const noexcept;
    void resetClock() noexcept;
    void reset(engine::GameSession session);
    [[nodiscard]] engine::GameSnapshot snapshot() const;
    [[nodiscard]] engine::SessionPhase phase() const noexcept;
    [[nodiscard]] std::uint64_t presentationSubSecondFrame() const noexcept;

  private:
    [[nodiscard]] engine::FrameAdvanceResult advanceOneLogicalFrame(engine::GameCommand command);

    engine::GameSession session_;
    std::uint64_t accumulatedSubSecondTickCredits_ = 0;
    std::uint64_t presentationSubSecondFrame_ = 0;
    std::size_t maximumLogicalFramesPerAdvance_ = 0;
    bool paused_ = false;
};

} // namespace boulderdash::app
