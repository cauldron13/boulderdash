#include "engine/RockfordRules.h"

#include "engine/ObjectCodes.h"
#include "engine/ProgressionRules.h"

#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

bool selectMoveOffset(const Direction direction, std::ptrdiff_t &x, std::ptrdiff_t &y)
{
    switch (direction)
    {
    case Direction::South:
        y = 1;
        return true;
    case Direction::North:
        y = -1;
        return true;
    case Direction::East:
    case Direction::NorthEast:
    case Direction::SouthEast:
        x = 1;
        return true;
    case Direction::West:
    case Direction::NorthWest:
    case Direction::SouthWest:
        x = -1;
        return true;
    case Direction::Neutral:
        return false;
    }

    return false;
}

} // namespace

void processRockfordAppearance(CaveUpdateContext &context, const CellPosition position)
{
    if (!context.state.runtime.has_value() || context.state.runtime->appearanceCountdown != 0)
    {
        return;
    }

    const CellCode cell = context.state.grid.at(position);
    if (cell == objectcodes::kInbox)
    {
        context.state.grid.set(position, objectcodes::kPreRockfordStage1);
    }
    else if (cell == objectcodes::kPreRockfordStage1)
    {
        context.state.grid.set(position, objectcodes::kPreRockfordStage2);
    }
    else if (cell == objectcodes::kPreRockfordStage2)
    {
        context.state.grid.set(position, objectcodes::kPreRockfordStage3);
    }
    else if (cell == objectcodes::kPreRockfordStage3)
    {
        context.state.grid.set(position, objectcodes::kRockford);
        context.state.runtime->rockfordPosition = position;
        context.state.phase = SessionPhase::Playing;
    }
}

void processInAndOutBoxes(CaveUpdateContext &context, const CellPosition position)
{
    if (!context.state.runtime.has_value())
    {
        throw std::logic_error("In and out box processing requires a cave runtime state.");
    }

    ++context.state.campaign.flashingEntryBoxState;
    if ((context.state.campaign.flashingEntryBoxState & 0x01U) == 0 &&
        context.state.grid.at(position) == objectcodes::kInbox)
    {
        processRockfordAppearance(context, position);
    }
}

void processRockford(CaveUpdateContext &context, const CellPosition position)
{
    if (!context.state.runtime.has_value())
    {
        return;
    }

    context.state.runtime->rockfordDeadTicks = 0;

    if (!context.state.cave.has_value())
    {
        throw std::logic_error("A playing cave requires immutable cave metadata.");
    }

    std::ptrdiff_t deltaX = 0;
    std::ptrdiff_t deltaY = 0;
    if (!selectMoveOffset(context.command.direction, deltaX, deltaY))
    {
        return;
    }

    const std::ptrdiff_t targetX = static_cast<std::ptrdiff_t>(position.x) + deltaX;
    const std::ptrdiff_t targetY = static_cast<std::ptrdiff_t>(position.y) + deltaY;
    if (targetX < 0 || targetY < 0 ||
        !context.state.grid.contains({static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)}))
    {
        return;
    }

    const CellPosition target{static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)};
    CellCode targetCode = context.state.grid.at(target);
    const bool targetWasEmpty = targetCode == objectcodes::kEmpty;
    if (targetCode == objectcodes::kStationaryBoulder)
    {
        if (deltaY != 0)
        {
            return;
        }
        const std::ptrdiff_t pushedX = targetX + deltaX;
        if (pushedX < 0 || !context.state.grid.contains({static_cast<std::size_t>(pushedX), target.y}) ||
            context.state.grid.at({static_cast<std::size_t>(pushedX), target.y}) != objectcodes::kEmpty ||
            (context.state.runtime->timeBasedRandom.next(true) & 0x03U) != 0)
        {
            return;
        }
        context.state.grid.set({static_cast<std::size_t>(pushedX), target.y}, objectcodes::kScannedStationaryBoulder);
        context.state.grid.set(target, objectcodes::kEmpty);
        emitGameEvent(context, GameEventType::BoulderPushed, target);
        targetCode = objectcodes::kEmpty;
    }
    if (targetCode != objectcodes::kEmpty && targetCode != objectcodes::kDirt &&
        targetCode != objectcodes::kStationaryDiamond && targetCode != objectcodes::kOpenOutbox)
    {
        return;
    }

    CaveRuntimeState &runtime = *context.state.runtime;
    if (targetCode == objectcodes::kDirt)
    {
        emitGameEvent(context, GameEventType::DugDirt, target);
    }
    if (targetCode == objectcodes::kStationaryDiamond)
    {
        const bool wasExitOpen = runtime.exitOpen;
        collectDiamond(context.state);
        emitGameEvent(context, GameEventType::DiamondCollected, target);
        if (!wasExitOpen && runtime.exitOpen)
        {
            emitGameEvent(context, GameEventType::DiamondQuotaReached, target);
        }
    }

    if (targetCode == objectcodes::kOpenOutbox)
    {
        runtime.enteredExit = true;
        runtime.exitReason = CaveExitReason::Completed;
        runtime.completionState = CaveCompletionState::ScoringTimeBonus;
        runtime.completionCycleCredits = 0;
        context.state.phase = SessionPhase::CaveCompleted;
        emitGameEvent(context, GameEventType::ExitEntered, target);
    }

    if (context.command.firePressed)
    {
        context.state.grid.set(target, objectcodes::kEmpty);
        return;
    }

    if (targetWasEmpty)
    {
        emitGameEvent(context, GameEventType::RockfordMovedThroughEmptySpace, target);
    }

    context.state.grid.set(target, objectcodes::kScannedRockford);
    context.state.grid.set(position, objectcodes::kEmpty);
    runtime.rockfordPosition = target;
}

} // namespace boulderdash::engine
