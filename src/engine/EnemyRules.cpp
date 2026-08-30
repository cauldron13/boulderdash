#include "engine/EnemyRules.h"

#include "engine/ObjectCodes.h"

#include <array>

namespace boulderdash::engine
{
namespace
{

struct Offset final
{
    std::ptrdiff_t x;
    std::ptrdiff_t y;
};

constexpr std::array<Offset, 4> kFireflyForward = {Offset{-1, 0}, Offset{0, -1}, Offset{1, 0}, Offset{0, 1}};
constexpr std::array<Offset, 4> kFireflyLeft = {Offset{0, 1}, Offset{-1, 0}, Offset{0, -1}, Offset{1, 0}};
constexpr std::array<Offset, 4> kButterflyForward = {Offset{0, 1}, Offset{-1, 0}, Offset{0, -1}, Offset{1, 0}};
constexpr std::array<Offset, 4> kButterflyRight = {Offset{-1, 0}, Offset{0, -1}, Offset{1, 0}, Offset{0, 1}};

bool isContactTarget(const CellCode code)
{
    return code == objectcodes::kRockford || code == objectcodes::kScannedRockford || code == objectcodes::kAmoeba;
}

bool hasContactTarget(const CaveGrid &grid, const CellPosition position)
{
    constexpr std::array<Offset, 4> offsets = {Offset{0, -1}, Offset{-1, 0}, Offset{1, 0}, Offset{0, 1}};
    for (const Offset offset : offsets)
    {
        const std::ptrdiff_t x = static_cast<std::ptrdiff_t>(position.x) + offset.x;
        const std::ptrdiff_t y = static_cast<std::ptrdiff_t>(position.y) + offset.y;
        if (x >= 0 && y >= 0 && grid.contains({static_cast<std::size_t>(x), static_cast<std::size_t>(y)}) &&
            isContactTarget(grid.at({static_cast<std::size_t>(x), static_cast<std::size_t>(y)})))
        {
            return true;
        }
    }
    return false;
}

bool tryMove(CaveGrid &grid, const CellPosition source, const Offset offset, const CellCode destinationCode)
{
    const std::ptrdiff_t x = static_cast<std::ptrdiff_t>(source.x) + offset.x;
    const std::ptrdiff_t y = static_cast<std::ptrdiff_t>(source.y) + offset.y;
    if (x < 0 || y < 0 || !grid.contains({static_cast<std::size_t>(x), static_cast<std::size_t>(y)}))
    {
        return false;
    }

    const CellPosition target{static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
    if (grid.at(target) != objectcodes::kEmpty)
    {
        return false;
    }

    grid.set(target, destinationCode);
    grid.set(source, objectcodes::kEmpty);
    return true;
}

void explode(CaveUpdateContext &context, const CellPosition center, const CellCode firstPhase)
{
    CaveGrid &grid = context.state.grid;
    emitGameEvent(context, GameEventType::Explosion, center);
    const CellCode laterPhase = static_cast<CellCode>(firstPhase - 1);
    for (std::ptrdiff_t y = -1; y <= 1; ++y)
    {
        for (std::ptrdiff_t x = -1; x <= 1; ++x)
        {
            const std::ptrdiff_t targetX = static_cast<std::ptrdiff_t>(center.x) + x;
            const std::ptrdiff_t targetY = static_cast<std::ptrdiff_t>(center.y) + y;
            if (targetX < 0 || targetY < 0 ||
                !grid.contains({static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)}))
            {
                continue;
            }
            const CellPosition target{static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)};
            const CellCode cell = grid.at(target);
            if (cell != objectcodes::kSteelWall)
            {
                if ((cell == objectcodes::kRockford || cell == objectcodes::kScannedRockford) &&
                    context.state.runtime.has_value())
                {
                    context.state.runtime->rockfordPosition.reset();
                    context.state.phase = SessionPhase::RockfordDead;
                    emitGameEvent(context, GameEventType::RockfordDied,
                                  {static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)});
                }
                grid.set(target, (y < 0 || (y == 0 && x <= 0)) ? firstPhase : laterPhase);
            }
        }
    }
}

void explodeDown(CaveUpdateContext &context, const CellPosition topCenter, const CellCode firstPhase)
{
    CaveGrid &grid = context.state.grid;
    emitGameEvent(context, GameEventType::Explosion, topCenter);
    const CellCode laterPhase = static_cast<CellCode>(firstPhase - 1);
    for (std::ptrdiff_t y = 0; y <= 2; ++y)
    {
        for (std::ptrdiff_t x = -1; x <= 1; ++x)
        {
            const std::ptrdiff_t targetX = static_cast<std::ptrdiff_t>(topCenter.x) + x;
            const std::ptrdiff_t targetY = static_cast<std::ptrdiff_t>(topCenter.y) + y;
            if (targetX < 0 || targetY < 0 ||
                !grid.contains({static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)}))
            {
                continue;
            }
            const CellPosition target{static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)};
            const CellCode cell = grid.at(target);
            if (cell == objectcodes::kSteelWall)
            {
                continue;
            }
            if ((cell == objectcodes::kRockford || cell == objectcodes::kScannedRockford) &&
                context.state.runtime.has_value())
            {
                context.state.runtime->rockfordPosition.reset();
                context.state.phase = SessionPhase::RockfordDead;
                emitGameEvent(context, GameEventType::RockfordDied,
                              {static_cast<std::size_t>(targetX), static_cast<std::size_t>(targetY)});
            }
            grid.set(target, (y == 0 && x <= 0) ? firstPhase : laterPhase);
        }
    }
}

} // namespace

void processFirefly(CaveUpdateContext &context, const CellPosition position)
{
    CaveGrid &grid = context.state.grid;
    if (hasContactTarget(grid, position))
    {
        explode(context, position, objectcodes::kExplosionToSpaceStage1);
        return;
    }

    const std::uint8_t direction = static_cast<std::uint8_t>(grid.at(position) & 0x03U);
    const std::uint8_t leftDirection = static_cast<std::uint8_t>((direction + 3U) & 0x03U);
    if (tryMove(grid, position, kFireflyLeft[direction], static_cast<CellCode>(0x0CU + leftDirection)))
    {
        return;
    }
    if (tryMove(grid, position, kFireflyForward[direction], static_cast<CellCode>(0x0CU + direction)))
    {
        return;
    }
    grid.set(position, static_cast<CellCode>(objectcodes::kFireflyLeft + ((direction + 1U) & 0x03U)));
}

void processButterfly(CaveUpdateContext &context, const CellPosition position)
{
    CaveGrid &grid = context.state.grid;
    if (hasContactTarget(grid, position))
    {
        explode(context, position, objectcodes::kExplosionToDiamondStage1);
        return;
    }

    const std::uint8_t direction = static_cast<std::uint8_t>(grid.at(position) & 0x03U);
    const std::uint8_t rightDirection = static_cast<std::uint8_t>((direction + 1U) & 0x03U);
    if (tryMove(grid, position, kButterflyRight[direction], static_cast<CellCode>(0x34U + rightDirection)))
    {
        return;
    }
    if (tryMove(grid, position, kButterflyForward[direction], static_cast<CellCode>(0x34U + direction)))
    {
        return;
    }
    grid.set(position, static_cast<CellCode>(0x34U + ((direction + 3U) & 0x03U)));
}

void processExplosion(CaveUpdateContext &context, const CellPosition position)
{
    CaveGrid &grid = context.state.grid;
    switch (grid.at(position))
    {
    case 0x1B:
        grid.set(position, objectcodes::kExplosionToSpaceStage1);
        break;
    case 0x1C:
        grid.set(position, 0x1D);
        break;
    case 0x1D:
        grid.set(position, 0x1E);
        break;
    case 0x1E:
        grid.set(position, 0x1F);
        break;
    case 0x1F:
        grid.set(position, objectcodes::kEmpty);
        break;
    case 0x20:
        grid.set(position, objectcodes::kExplosionToDiamondStage1);
        break;
    case 0x21:
        grid.set(position, 0x22);
        break;
    case 0x22:
        grid.set(position, 0x23);
        break;
    case 0x23:
        grid.set(position, 0x24);
        break;
    case 0x24:
        grid.set(position, objectcodes::kStationaryDiamond);
        break;
    default:
        break;
    }
}

void explodeFallingObject(CaveUpdateContext &context, const CellPosition position, const bool producesDiamonds)
{
    explodeDown(context, position,
                producesDiamonds ? objectcodes::kExplosionToDiamondStage1 : objectcodes::kExplosionToSpaceStage1);
}

} // namespace boulderdash::engine
