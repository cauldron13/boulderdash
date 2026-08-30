#pragma once

#include "engine/EngineTypes.h"
#include "engine/GameState.h"

#include <cstdint>
#include <vector>

namespace boulderdash::engine
{

struct FrameAdvanceResult final
{
    std::vector<GameEvent> events;
    bool caveLoaded = false;
    bool presentationChanged = false;
};

class GameSession final
{
  public:
    explicit GameSession(GameState initialState);

    [[nodiscard]] GameSnapshot snapshot() const;
    [[nodiscard]] SessionPhase phase() const noexcept;
    [[nodiscard]] FrameAdvanceResult advanceFrame(GameCommand command);

  private:
    [[nodiscard]] std::vector<GameEvent> tick(GameCommand command);
    void loadPendingCave();
    void advanceCaveSecond(std::vector<GameEvent> &events);
    void advanceLogicalFrame();
    void advanceCaveCompletionCycles(std::uint64_t cycles);

    GameState state_;
    std::uint32_t framesSinceCaveSecond_ = 0;
    std::uint64_t caveTickCredits_ = 0;
    std::uint64_t caveCompletionCycleRemainder_ = 0;
    std::uint64_t caveTransitionCycleRemainder_ = 0;
    std::uint64_t caveTransitionCycleCredits_ = 0;
};

[[nodiscard]] GameSession makeNewCampaignSession(std::uint8_t playerCount);

} // namespace boulderdash::engine
