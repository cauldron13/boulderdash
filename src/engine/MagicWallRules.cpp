#include "engine/MagicWallRules.h"

#include "engine/EngineTiming.h"
#include "engine/ObjectCodes.h"

#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

MagicWallRuntimeState &magicWallState(GameState &state)
{
    if (!state.runtime.has_value())
    {
        throw std::logic_error("Magic wall processing requires a cave runtime state.");
    }

    return state.runtime->magicWall;
}

} // namespace

bool processFallingObjectAtMagicWall(CaveUpdateContext &context, const CellPosition position,
                                     const CellCode scannedConvertedObject)
{
    CaveGrid &grid = context.state.grid;
    const CaveSize size = grid.size();
    if (position.y + 1 >= size.height || grid.at({position.x, position.y + 1}) != objectcodes::kMagicWall)
    {
        return false;
    }

    MagicWallRuntimeState &magicWall = magicWallState(context.state);
    if (magicWall.state == MagicWallState::Inactive)
    {
        magicWall.state = MagicWallState::Active;
        emitGameEvent(context, GameEventType::MagicWallActivated, {position.x, position.y + 1});
    }

    const CellPosition source = position;
    const CellPosition destination{position.x, position.y + 2};
    if (magicWall.state == MagicWallState::Active && destination.y < size.height &&
        grid.at(destination) == objectcodes::kEmpty)
    {
        grid.set(destination, scannedConvertedObject);
    }

    // Both ProcessFallingDiamond and ProcessFallingBoulder clear the source,
    // including when the wall is expired or the destination is occupied.
    grid.set(source, objectcodes::kEmpty);
    return true;
}

void advanceMagicWallFrame(GameState &state)
{
    if (!state.runtime.has_value())
    {
        return;
    }

    MagicWallRuntimeState &magicWall = state.runtime->magicWall;
    if (magicWall.state == MagicWallState::Expired)
    {
        magicWall.state = MagicWallState::Finished;
        return;
    }
    if (magicWall.state != MagicWallState::Active)
    {
        return;
    }
    if (!state.cave.has_value())
    {
        throw std::logic_error("An active magic wall requires cave metadata.");
    }

    ++magicWall.activeFrameCount;
    if (magicWall.activeFrameCount != kC64SubSecondTicksPerGameSecond)
    {
        return;
    }

    magicWall.activeFrameCount = 0;
    ++magicWall.activeSeconds;
    if (magicWall.activeSeconds == state.cave->configuration.magicWallMillingTimeOrAmoeba3PercentMax)
    {
        magicWall.state = MagicWallState::Expired;
    }
}

} // namespace boulderdash::engine
