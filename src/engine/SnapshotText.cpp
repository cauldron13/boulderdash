#include "engine/SnapshotText.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

const char *directionName(const Direction direction)
{
    switch (direction)
    {
    case Direction::Neutral:
        return "neutral";
    case Direction::North:
        return "north";
    case Direction::NorthEast:
        return "northEast";
    case Direction::East:
        return "east";
    case Direction::SouthEast:
        return "southEast";
    case Direction::South:
        return "south";
    case Direction::SouthWest:
        return "southWest";
    case Direction::West:
        return "west";
    case Direction::NorthWest:
        return "northWest";
    }

    throw std::logic_error("The game command contains an invalid direction.");
}

const char *phaseName(const SessionPhase phase)
{
    switch (phase)
    {
    case SessionPhase::Uninitialized:
        return "uninitialized";
    case SessionPhase::CavePrepared:
        return "cavePrepared";
    case SessionPhase::RockfordAppearing:
        return "rockfordAppearing";
    case SessionPhase::Playing:
        return "playing";
    case SessionPhase::CaveCompleted:
        return "caveCompleted";
    case SessionPhase::CaveTransitioning:
        return "caveTransitioning";
    case SessionPhase::RockfordDead:
        return "rockfordDead";
    case SessionPhase::Transitioning:
        return "transitioning";
    case SessionPhase::GameOver:
        return "gameOver";
    }

    throw std::logic_error("The game snapshot contains an invalid session phase.");
}

const char *caveCompletionStateName(const CaveCompletionState state)
{
    switch (state)
    {
    case CaveCompletionState::Inactive:
        return "inactive";
    case CaveCompletionState::ScoringTimeBonus:
        return "scoringTimeBonus";
    case CaveCompletionState::PostScoreDelay:
        return "postScoreDelay";
    }

    throw std::logic_error("The game snapshot contains an invalid cave-completion state.");
}

const char *magicWallStateName(const MagicWallState state)
{
    switch (state)
    {
    case MagicWallState::Inactive:
        return "inactive";
    case MagicWallState::Active:
        return "active";
    case MagicWallState::Expired:
        return "expired";
    case MagicWallState::Finished:
        return "finished";
    }

    throw std::logic_error("The game snapshot contains an invalid magic wall state.");
}

const char *caveExitReasonName(const CaveExitReason reason)
{
    switch (reason)
    {
    case CaveExitReason::None:
        return "none";
    case CaveExitReason::Completed:
        return "completed";
    case CaveExitReason::TimeExpired:
        return "timeExpired";
    case CaveExitReason::RockfordDied:
        return "rockfordDied";
    }

    throw std::logic_error("The game snapshot contains an invalid cave exit reason.");
}

void appendHexByte(std::ostringstream &stream, const CellCode cell)
{
    stream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(cell)
           << std::dec;
}

void appendGrid(std::ostringstream &stream, const GameSnapshot &snapshot)
{
    const CaveSize size = snapshot.grid.size();
    stream << "grid-encoding=hex-u8-row-major\n";
    stream << "grid:\n";

    for (std::size_t y = 0; y < size.height; ++y)
    {
        for (std::size_t x = 0; x < size.width; ++x)
        {
            appendHexByte(stream, snapshot.grid.at({x, y}));
        }
        stream << '\n';
    }
}

void appendCaveMetadata(std::ostringstream &stream, const std::optional<CaveMetadata> &cave)
{
    if (!cave.has_value())
    {
        stream << "cave=none\n";
        return;
    }

    stream << "cave=prepared\n";
    stream << "cave-number=" << static_cast<unsigned int>(cave->caveNumber) << '\n';
    stream << "cave-sublevel-index=" << static_cast<unsigned int>(cave->sublevelIndex) << '\n';
    if (cave->rockfordEntry.has_value())
    {
        stream << "rockford-entry=" << cave->rockfordEntry->x << ',' << cave->rockfordEntry->y << '\n';
    }
    else
    {
        stream << "rockford-entry=none\n";
    }
    stream << "magic-wall-milling-time-or-amoeba-3-percent-max="
           << static_cast<unsigned int>(cave->configuration.magicWallMillingTimeOrAmoeba3PercentMax) << '\n';
    stream << "initial-diamond-value=" << static_cast<unsigned int>(cave->configuration.initialDiamondValue) << '\n';
    stream << "extra-diamond-value=" << static_cast<unsigned int>(cave->configuration.extraDiamondValue) << '\n';
    stream << "diamonds-required=" << static_cast<unsigned int>(cave->configuration.diamondsRequired) << '\n';
    stream << "cave-time=" << static_cast<unsigned int>(cave->configuration.caveTime) << '\n';
    stream << "background-colour-1=" << static_cast<unsigned int>(cave->configuration.backgroundColour1) << '\n';
    stream << "background-colour-2=" << static_cast<unsigned int>(cave->configuration.backgroundColour2) << '\n';
    stream << "foreground-colour=" << static_cast<unsigned int>(cave->configuration.foregroundColour) << '\n';
    stream << "reserved-bytes=" << static_cast<unsigned int>(cave->configuration.reservedBytes[0]) << ','
           << static_cast<unsigned int>(cave->configuration.reservedBytes[1]) << '\n';
    stream << "random-seed-1=" << static_cast<unsigned int>(cave->randomSeed1) << '\n';
    stream << "random-seed-2=" << static_cast<unsigned int>(cave->randomSeed2) << '\n';
}

void appendRuntimeState(std::ostringstream &stream, const std::optional<CaveRuntimeState> &runtime)
{
    if (!runtime.has_value())
    {
        stream << "runtime=none\n";
        return;
    }

    stream << "runtime=active\n";
    if (runtime->rockfordPosition.has_value())
    {
        stream << "rockford-position=" << runtime->rockfordPosition->x << ',' << runtime->rockfordPosition->y << '\n';
    }
    else
    {
        stream << "rockford-position=none\n";
    }
    stream << "collected-diamonds=" << runtime->collectedDiamonds << '\n';
    stream << "score=" << runtime->score << '\n';
    stream << "exit-open=" << (runtime->exitOpen ? "true" : "false") << '\n';
    stream << "entered-exit=" << (runtime->enteredExit ? "true" : "false") << '\n';
    stream << "appearance-countdown=" << static_cast<unsigned int>(runtime->appearanceCountdown) << '\n';
    stream << "time-remaining-seconds=" << runtime->timeRemainingSeconds << '\n';
    stream << "rockford-dead-ticks=" << static_cast<unsigned int>(runtime->rockfordDeadTicks) << '\n';
    stream << "cave-exit-reason=" << caveExitReasonName(runtime->exitReason) << '\n';
    stream << "cave-completion-state=" << caveCompletionStateName(runtime->completionState) << '\n';
    stream << "cave-completion-cycle-credits=" << runtime->completionCycleCredits << '\n';
    stream << "magic-wall-state=" << magicWallStateName(runtime->magicWall.state) << '\n';
    stream << "magic-wall-active-frame-count=" << static_cast<unsigned int>(runtime->magicWall.activeFrameCount)
           << '\n';
    stream << "magic-wall-active-seconds=" << static_cast<unsigned int>(runtime->magicWall.activeSeconds) << '\n';
    stream << "amoeba-cell-count-this-tick=" << static_cast<unsigned int>(runtime->amoeba.cellCountThisTick) << '\n';
    stream << "amoeba-cell-count-previous-tick=" << static_cast<unsigned int>(runtime->amoeba.cellCountPreviousTick)
           << '\n';
    stream << "amoeba-could-grow-this-tick=" << (runtime->amoeba.couldGrowThisTick ? "true" : "false") << '\n';
    stream << "amoeba-could-grow-last-tick=" << (runtime->amoeba.couldGrowLastTick ? "true" : "false") << '\n';
    stream << "amoeba-is-growing=" << (runtime->amoeba.isGrowing ? "true" : "false") << '\n';
    stream << "amoeba-growth-probability-mask=" << static_cast<unsigned int>(runtime->amoeba.growthProbabilityMask)
           << '\n';
    stream << "amoeba-cave-seconds-elapsed=" << static_cast<unsigned int>(runtime->amoeba.caveSecondsElapsed) << '\n';
    stream << "time-based-random-cia1-timer-a-low="
           << static_cast<unsigned int>(runtime->timeBasedRandom.sample.cia1TimerALow) << '\n';
    stream << "time-based-random-cia1-timer-a-high="
           << static_cast<unsigned int>(runtime->timeBasedRandom.sample.cia1TimerAHigh) << '\n';
    stream << "time-based-random-cia2-timer-a-low="
           << static_cast<unsigned int>(runtime->timeBasedRandom.sample.cia2TimerALow) << '\n';
    stream << "time-based-random-cia2-timer-a-high="
           << static_cast<unsigned int>(runtime->timeBasedRandom.sample.cia2TimerAHigh) << '\n';
    stream << "time-based-random-cia2-timer-b-low="
           << static_cast<unsigned int>(runtime->timeBasedRandom.sample.cia2TimerBLow) << '\n';
    stream << "time-based-random-cia2-timer-b-high="
           << static_cast<unsigned int>(runtime->timeBasedRandom.sample.cia2TimerBHigh) << '\n';
    stream << "time-based-random-calls-consumed=" << runtime->timeBasedRandom.callsConsumed << '\n';
}

void appendCampaignState(std::ostringstream &stream, const CampaignRuntimeState &campaign)
{
    stream << "campaign-player-count=" << static_cast<unsigned int>(campaign.playerCount) << '\n';
    stream << "campaign-current-player=" << static_cast<unsigned int>(campaign.currentPlayer) << '\n';
    stream << "campaign-sublevel-index=" << static_cast<unsigned int>(campaign.sublevelIndex) << '\n';
    stream << "campaign-pending-cave-number=" << static_cast<unsigned int>(campaign.pendingCaveNumber) << '\n';
    stream << "campaign-pending-cave-is-bonus=" << (campaign.pendingCaveIsBonus ? "true" : "false") << '\n';
    stream << "campaign-game-over=" << (campaign.gameOver ? "true" : "false") << '\n';
    stream << "campaign-flashing-entry-box-state=" << static_cast<unsigned int>(campaign.flashingEntryBoxState) << '\n';
    for (std::size_t playerIndex = 0; playerIndex < campaign.players.size(); ++playerIndex)
    {
        stream << "campaign-player-" << playerIndex << "-score=" << campaign.players[playerIndex].score << '\n';
        stream << "campaign-player-" << playerIndex
               << "-lives=" << static_cast<unsigned int>(campaign.players[playerIndex].lives) << '\n';
        stream << "campaign-player-" << playerIndex
               << "-cave-number=" << static_cast<unsigned int>(campaign.players[playerIndex].caveNumber) << '\n';
        stream << "campaign-player-" << playerIndex
               << "-sublevel-index=" << static_cast<unsigned int>(campaign.players[playerIndex].sublevelIndex) << '\n';
    }
}

} // namespace

std::string serializeSnapshotV1(const GameSnapshot &snapshot)
{
    const CaveSize size = snapshot.grid.size();
    std::ostringstream stream;
    stream << "format=boulderdash.snapshot.v1\n";
    stream << "width=" << size.width << '\n';
    stream << "height=" << size.height << '\n';
    stream << "tick=" << snapshot.tick << '\n';
    stream << "phase=" << phaseName(snapshot.phase) << '\n';
    stream << "command=" << directionName(snapshot.lastCommand.direction) << '\n';
    appendGrid(stream, snapshot);
    stream << "events:\n";
    return stream.str();
}

std::string serializeSnapshot(const GameSnapshot &snapshot)
{
    const CaveSize size = snapshot.grid.size();
    std::ostringstream stream;
    stream << "format=boulderdash.snapshot.v2\n";
    stream << "width=" << size.width << '\n';
    stream << "height=" << size.height << '\n';
    stream << "tick=" << snapshot.tick << '\n';
    stream << "phase=" << phaseName(snapshot.phase) << '\n';
    stream << "command=" << directionName(snapshot.lastCommand.direction) << '\n';
    stream << "fire=" << (snapshot.lastCommand.firePressed ? "pressed" : "released") << '\n';
    appendCaveMetadata(stream, snapshot.cave);
    appendRuntimeState(stream, snapshot.runtime);
    appendCampaignState(stream, snapshot.campaign);
    appendGrid(stream, snapshot);

    stream << "events:\n";
    return stream.str();
}

std::string snapshotAsciiDiagnostic(const GameSnapshot &snapshot)
{
    const CaveSize size = snapshot.grid.size();
    std::ostringstream stream;
    stream << "grid-ascii-hex-u8:\n";

    for (std::size_t y = 0; y < size.height; ++y)
    {
        for (std::size_t x = 0; x < size.width; ++x)
        {
            if (x != 0)
            {
                stream << ' ';
            }
            appendHexByte(stream, snapshot.grid.at({x, y}));
        }
        stream << '\n';
    }

    return stream.str();
}

} // namespace boulderdash::engine
