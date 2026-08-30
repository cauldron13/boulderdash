#include "engine/ProgressionRules.h"

#include "engine/AmoebaRules.h"

#include <array>
#include <limits>
#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

constexpr std::uint32_t kC64ScoreModulo = 1000000;
constexpr std::uint8_t kC64MaximumLives = 9;
constexpr std::uint8_t kC64RockfordDeathTicks = 16;

constexpr std::array<CellCode, 21> kNextCaveByCurrentCave = {
    0, 2, 3, 4, 17, 6, 7, 8, 18, 10, 11, 12, 19, 14, 15, 16, 20, 5, 9, 13, 21,
};

bool isBonusCave(const CellCode caveNumber)
{
    return caveNumber >= 0x11 && caveNumber <= 0x14;
}

CaveRuntimeState &runtimeState(GameState &state)
{
    if (!state.runtime.has_value())
    {
        throw std::logic_error("Cave progression requires a cave runtime state.");
    }

    return *state.runtime;
}

const CaveMetadata &caveMetadata(const GameState &state)
{
    if (!state.cave.has_value())
    {
        throw std::logic_error("Cave progression requires immutable cave metadata.");
    }

    return *state.cave;
}

PlayerProgress &currentPlayer(GameState &state)
{
    CampaignRuntimeState &campaign = state.campaign;
    if (campaign.playerCount == 0 || campaign.playerCount > campaign.players.size() ||
        campaign.currentPlayer >= campaign.playerCount)
    {
        throw std::logic_error("The campaign player state is invalid.");
    }

    return campaign.players[campaign.currentPlayer];
}

void synchronizeCurrentPlayerScore(GameState &state)
{
    runtimeState(state).score = currentPlayer(state).score;
}

void grantLife(PlayerProgress &player)
{
    if (player.lives < kC64MaximumLives)
    {
        ++player.lives;
    }
}

void addScore(GameState &state, const std::uint32_t increment)
{
    PlayerProgress &player = currentPlayer(state);
    const std::uint32_t oldScore = player.score;
    const std::uint32_t oldThousandsDigit = (oldScore / 1000U) % 10U;
    const std::uint32_t oldHundredsDigit = (oldScore / 100U) % 10U;
    player.score = static_cast<std::uint32_t>((static_cast<std::uint64_t>(oldScore) + increment) % kC64ScoreModulo);

    const std::uint32_t newThousandsDigit = (player.score / 1000U) % 10U;
    const std::uint32_t newHundredsDigit = (player.score / 100U) % 10U;
    if (oldThousandsDigit != newThousandsDigit)
    {
        grantLife(player);
    }
    if (oldHundredsDigit == 4U && oldHundredsDigit != newHundredsDigit)
    {
        grantLife(player);
    }

    synchronizeCurrentPlayerScore(state);
}

void selectNextCave(GameState &state, const SessionPhase nextPhase)
{
    const CellCode currentCave = caveMetadata(state).caveNumber;
    if (currentCave == 0 || currentCave >= kNextCaveByCurrentCave.size())
    {
        throw std::logic_error("The campaign cave number is outside the C64 sequence.");
    }

    CampaignRuntimeState &campaign = state.campaign;
    PlayerProgress &player = currentPlayer(state);
    const CellCode next = kNextCaveByCurrentCave[currentCave];
    if (next == 0x15)
    {
        if (campaign.sublevelIndex < 4)
        {
            ++campaign.sublevelIndex;
        }
        campaign.pendingCaveNumber = 1;
    }
    else
    {
        campaign.pendingCaveNumber = next;
    }
    player.caveNumber = campaign.pendingCaveNumber;
    player.sublevelIndex = campaign.sublevelIndex;
    campaign.pendingCaveIsBonus = isBonusCave(campaign.pendingCaveNumber);
    if (campaign.pendingCaveIsBonus)
    {
        grantLife(player);
    }
    state.phase = nextPhase;
}

void failCurrentCave(GameState &state, const CaveExitReason reason)
{
    CaveRuntimeState &runtime = runtimeState(state);
    runtime.exitReason = reason;
    if (isBonusCave(caveMetadata(state).caveNumber))
    {
        selectNextCave(state, SessionPhase::Transitioning);
        return;
    }

    CampaignRuntimeState &campaign = state.campaign;
    PlayerProgress &failedPlayer = currentPlayer(state);
    if (failedPlayer.lives == 0)
    {
        throw std::logic_error("A living cave cannot remove a life from a player with no lives.");
    }
    failedPlayer.caveNumber = caveMetadata(state).caveNumber;
    failedPlayer.sublevelIndex = campaign.sublevelIndex;
    --failedPlayer.lives;

    if (campaign.playerCount == 2)
    {
        const std::uint8_t otherPlayer = static_cast<std::uint8_t>(campaign.currentPlayer ^ 1U);
        if (campaign.players[otherPlayer].lives != 0)
        {
            campaign.currentPlayer = otherPlayer;
        }
    }

    bool anyPlayerAlive = false;
    for (std::uint8_t playerIndex = 0; playerIndex < campaign.playerCount; ++playerIndex)
    {
        anyPlayerAlive = anyPlayerAlive || campaign.players[playerIndex].lives != 0;
    }
    if (!anyPlayerAlive)
    {
        campaign.gameOver = true;
        campaign.currentPlayer = 0;
        campaign.players[0].lives = 3;
        state.phase = SessionPhase::GameOver;
        return;
    }

    const PlayerProgress &nextPlayer = currentPlayer(state);
    campaign.sublevelIndex = nextPlayer.sublevelIndex;
    campaign.pendingCaveNumber = nextPlayer.caveNumber;
    campaign.pendingCaveIsBonus = isBonusCave(campaign.pendingCaveNumber);
    synchronizeCurrentPlayerScore(state);
    state.phase = SessionPhase::Transitioning;
}

} // namespace

void initializeCaveProgress(GameState &state)
{
    CaveRuntimeState &runtime = runtimeState(state);
    const CaveMetadata &cave = caveMetadata(state);
    CampaignRuntimeState &campaign = state.campaign;
    if (campaign.pendingCaveNumber == 0)
    {
        campaign.pendingCaveNumber = cave.caveNumber;
        campaign.pendingCaveIsBonus = isBonusCave(cave.caveNumber);
        campaign.players[campaign.currentPlayer].score = runtime.score;
    }
    PlayerProgress &player = currentPlayer(state);
    player.caveNumber = cave.caveNumber;
    player.sublevelIndex = cave.sublevelIndex;
    campaign.sublevelIndex = cave.sublevelIndex;

    runtime.timeRemainingSeconds = cave.configuration.caveTime;
    runtime.rockfordDeadTicks = 0;
    runtime.exitReason = CaveExitReason::None;
    runtime.completionState = CaveCompletionState::Inactive;
    runtime.completionCycleCredits = 0;
    synchronizeCurrentPlayerScore(state);
}

void collectDiamond(GameState &state)
{
    CaveRuntimeState &runtime = runtimeState(state);
    const CaveConfiguration &configuration = caveMetadata(state).configuration;
    addScore(state, runtime.exitOpen ? configuration.extraDiamondValue : configuration.initialDiamondValue);
    ++runtime.collectedDiamonds;
    if (runtime.collectedDiamonds == configuration.diamondsRequired)
    {
        runtime.exitOpen = true;
    }
}

void advanceActiveCaveSecond(GameState &state)
{
    CaveRuntimeState &runtime = runtimeState(state);
    if (runtime.timeRemainingSeconds == 0)
    {
        failCurrentCave(state, CaveExitReason::TimeExpired);
        return;
    }

    --runtime.timeRemainingSeconds;
    advanceAmoebaSecond(state);
    if (runtime.timeRemainingSeconds == 0)
    {
        failCurrentCave(state, CaveExitReason::TimeExpired);
    }
}

void advanceCompletedCaveCycles(GameState &state, const std::uint64_t cycles)
{
    CaveRuntimeState &runtime = runtimeState(state);
    if (state.phase != SessionPhase::CaveCompleted)
    {
        return;
    }
    if (cycles > std::numeric_limits<std::uint64_t>::max() - runtime.completionCycleCredits)
    {
        throw std::overflow_error("The cave-completion cycle accumulator would overflow.");
    }
    runtime.completionCycleCredits += cycles;

    while (true)
    {
        switch (runtime.completionState)
        {
        case CaveCompletionState::Inactive:
            return;
        case CaveCompletionState::ScoringTimeBonus:
            runtime.exitReason = CaveExitReason::Completed;
            if (runtime.timeRemainingSeconds == 0)
            {
                runtime.completionState = CaveCompletionState::PostScoreDelay;
                continue;
            }
            if (runtime.completionCycleCredits < kC64PalCaveCompletionScoreLoopCycles)
            {
                return;
            }
            runtime.completionCycleCredits -= kC64PalCaveCompletionScoreLoopCycles;
            --runtime.timeRemainingSeconds;
            addScore(state, static_cast<std::uint32_t>(state.campaign.sublevelIndex) + 1U);
            if (runtime.timeRemainingSeconds == 0)
            {
                runtime.completionState = CaveCompletionState::PostScoreDelay;
            }
            continue;
        case CaveCompletionState::PostScoreDelay:
            if (runtime.completionCycleCredits < kC64PalCaveCompletionPostScoreDelayCycles)
            {
                return;
            }
            runtime.completionCycleCredits -= kC64PalCaveCompletionPostScoreDelayCycles;
            selectNextCave(state, SessionPhase::Transitioning);
            runtime.completionState = CaveCompletionState::Inactive;
            return;
        }
    }
}

void advanceRockfordDeath(GameState &state)
{
    CaveRuntimeState &runtime = runtimeState(state);
    if (runtime.rockfordDeadTicks < kC64RockfordDeathTicks)
    {
        ++runtime.rockfordDeadTicks;
    }
    // Intentional porting divergence: keyboard play leaves the cave
    // automatically after the C64 death delay. Do not restore DeathClick's
    // fire-button wait ($7dc3-$7dd6) without revisiting this input decision.
    if (runtime.rockfordDeadTicks == kC64RockfordDeathTicks)
    {
        failCurrentCave(state, CaveExitReason::RockfordDied);
    }
}

} // namespace boulderdash::engine
