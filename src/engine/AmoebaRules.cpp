#include "engine/AmoebaRules.h"

#include "engine/ObjectCodes.h"

#include <array>
#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

struct Offset final
{
    std::ptrdiff_t x;
    std::ptrdiff_t y;
};

// ProcessAmoeba at $6fd0 selects random targets in this order. Its confinement
// loop reads the same table backwards: down, right, left, up.
constexpr std::array<Offset, 4> kAmoebaOffsets = {
    Offset{0, -1},
    Offset{-1, 0},
    Offset{1, 0},
    Offset{0, 1},
};

bool canGrowInto(const CaveGrid &grid, const CellPosition source, const Offset offset)
{
    const std::ptrdiff_t x = static_cast<std::ptrdiff_t>(source.x) + offset.x;
    const std::ptrdiff_t y = static_cast<std::ptrdiff_t>(source.y) + offset.y;
    if (x < 0 || y < 0 || !grid.contains({static_cast<std::size_t>(x), static_cast<std::size_t>(y)}))
    {
        return false;
    }

    const CellCode code = grid.at({static_cast<std::size_t>(x), static_cast<std::size_t>(y)});
    return code == objectcodes::kEmpty || code == objectcodes::kDirt;
}

AmoebaRuntimeState &amoebaState(GameState &state)
{
    if (!state.runtime.has_value())
    {
        throw std::logic_error("Amoeba processing requires a cave runtime state.");
    }

    return state.runtime->amoeba;
}

} // namespace

void prepareAmoebaTick(GameState &state)
{
    AmoebaRuntimeState &amoeba = amoebaState(state);
    if (!amoeba.couldGrowThisTick && amoeba.isGrowing)
    {
        amoeba.couldGrowLastTick = false;
    }

    amoeba.isGrowing = amoeba.couldGrowThisTick;
    amoeba.couldGrowThisTick = false;
    amoeba.cellCountPreviousTick = amoeba.cellCountThisTick;
    amoeba.cellCountThisTick = 0;
}

void processAmoeba(CaveUpdateContext &context, const CellPosition position)
{
    CaveGrid &grid = context.state.grid;
    AmoebaRuntimeState &amoeba = amoebaState(context.state);
    ++amoeba.cellCountThisTick;

    if (amoeba.cellCountPreviousTick >= 0xc8)
    {
        grid.set(position, objectcodes::kScannedStationaryBoulder);
        return;
    }

    if (!amoeba.couldGrowLastTick)
    {
        grid.set(position, objectcodes::kStationaryDiamond);
        return;
    }

    const bool couldGrowAtEntry = amoeba.couldGrowThisTick;
    if (!couldGrowAtEntry)
    {
        for (const Offset offset : kAmoebaOffsets)
        {
            if (canGrowInto(grid, position, offset))
            {
                amoeba.couldGrowThisTick = true;
                break;
            }
        }
    }

    // ProcessAmoeba $6fee-$7012 preserves the scanner dispatch carry when
    // growth was known at entry; otherwise CPX #$00 leaves C set.
    const bool inputCarry = !couldGrowAtEntry;
    const std::uint8_t direction = static_cast<std::uint8_t>(context.state.runtime->timeBasedRandom.next(inputCarry) &
                                                             amoeba.growthProbabilityMask);
    if (direction >= kAmoebaOffsets.size() || !canGrowInto(grid, position, kAmoebaOffsets[direction]))
    {
        return;
    }

    const Offset offset = kAmoebaOffsets[direction];
    const CellPosition target{static_cast<std::size_t>(static_cast<std::ptrdiff_t>(position.x) + offset.x),
                              static_cast<std::size_t>(static_cast<std::ptrdiff_t>(position.y) + offset.y)};
    grid.set(target, objectcodes::kScannedAmoeba);
}

void advanceAmoebaSecond(GameState &state)
{
    if (!state.runtime.has_value() || !state.cave.has_value())
    {
        return;
    }

    AmoebaRuntimeState &amoeba = state.runtime->amoeba;
    ++amoeba.caveSecondsElapsed;
    if (amoeba.caveSecondsElapsed == state.cave->configuration.magicWallMillingTimeOrAmoeba3PercentMax)
    {
        amoeba.growthProbabilityMask = 0x0f;
    }
}

} // namespace boulderdash::engine
