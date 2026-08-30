#pragma once

#include "engine/CaveDefinition.h"
#include "engine/CaveGrid.h"
#include "engine/EngineTypes.h"
#include "engine/TimeBasedRandom.h"

#include <array>
#include <optional>

namespace boulderdash::engine
{

enum class MagicWallState : std::uint8_t
{
    Inactive = 0,
    Active = 1,
    Expired = 2,
    Finished = 3,
};

enum class CaveExitReason : std::uint8_t
{
    None,
    Completed,
    TimeExpired,
    RockfordDied,
};

enum class CaveCompletionState : std::uint8_t
{
    Inactive,
    ScoringTimeBonus,
    PostScoreDelay,
};

struct PlayerProgress final
{
    std::uint32_t score = 0;
    std::uint8_t lives = 3;
    // LoseLife ($8774-$87b5) saves and restores Cave and Level with each player's active state.
    CellCode caveNumber = 1;
    std::uint8_t sublevelIndex = 0;
};

struct CampaignRuntimeState final
{
    std::array<PlayerProgress, 2> players;
    std::uint8_t playerCount = 1;
    std::uint8_t currentPlayer = 0;
    std::uint8_t sublevelIndex = 0;
    CellCode pendingCaveNumber = 0;
    bool pendingCaveIsBonus = false;
    bool gameOver = false;
    std::uint8_t flashingEntryBoxState = 0;
};

struct MagicWallRuntimeState final
{
    MagicWallState state = MagicWallState::Inactive;
    std::uint8_t activeFrameCount = 0;
    std::uint8_t activeSeconds = 0;
};

struct AmoebaRuntimeState final
{
    std::uint8_t cellCountThisTick = 0;
    std::uint8_t cellCountPreviousTick = 0;
    bool couldGrowThisTick = false;
    bool couldGrowLastTick = true;
    bool isGrowing = false;
    std::uint8_t growthProbabilityMask = 0x7f;
    std::uint8_t caveSecondsElapsed = 0;
};

struct CaveRuntimeState final
{
    std::optional<CellPosition> rockfordPosition;
    std::uint16_t collectedDiamonds = 0;
    std::uint32_t score = 0;
    bool exitOpen = false;
    bool enteredExit = false;
    std::uint8_t appearanceCountdown = 0;
    std::uint16_t timeRemainingSeconds = 0;
    std::uint8_t rockfordDeadTicks = 0;
    CaveExitReason exitReason = CaveExitReason::None;
    CaveCompletionState completionState = CaveCompletionState::Inactive;
    std::uint64_t completionCycleCredits = 0;
    MagicWallRuntimeState magicWall;
    AmoebaRuntimeState amoeba;
    TimeBasedRandomState timeBasedRandom;
};

struct GameState final
{
    CaveGrid grid;
    LogicalTick tick = 0;
    SessionPhase phase = SessionPhase::Uninitialized;
    GameCommand lastCommand;
    std::optional<CaveMetadata> cave;
    std::optional<CaveRuntimeState> runtime;
    CampaignRuntimeState campaign;
};

struct GameSnapshot final
{
    CaveGrid grid;
    LogicalTick tick = 0;
    SessionPhase phase = SessionPhase::Uninitialized;
    GameCommand lastCommand;
    std::optional<CaveMetadata> cave;
    std::optional<CaveRuntimeState> runtime;
    CampaignRuntimeState campaign;
};

} // namespace boulderdash::engine
