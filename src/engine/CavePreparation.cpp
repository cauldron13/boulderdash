#include "engine/CavePreparation.h"

#include "engine/PseudoRandom.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

constexpr CaveSize kCanonicalCaveSize{40, 22};
constexpr CellCode kDirt = 0x01;
constexpr CellCode kSteelWall = 0x07;
constexpr CellCode kInbox = 0x25;
constexpr CellCode kC64DrawCoordinateYOffset = 2;

struct DrawVector final
{
    std::ptrdiff_t x = 0;
    std::ptrdiff_t y = 0;
};

[[nodiscard]] DrawVector directionVector(const CellCode direction)
{
    constexpr std::array<DrawVector, 8> vectors = {
        DrawVector{0, -1}, DrawVector{1, -1}, DrawVector{1, 0},  DrawVector{1, 1},
        DrawVector{0, 1},  DrawVector{-1, 1}, DrawVector{-1, 0}, DrawVector{-1, -1},
    };

    if (direction >= vectors.size())
    {
        throw std::invalid_argument("A cave line command contains an invalid direction.");
    }

    return vectors[direction];
}

[[nodiscard]] DrawVector projectDrawPosition(const CellCode x, const CellCode c64Y)
{
    if (x >= kCanonicalCaveSize.width || c64Y < kC64DrawCoordinateYOffset)
    {
        throw std::invalid_argument("A cave drawing command starts outside the logical cave.");
    }

    const std::size_t y = static_cast<std::size_t>(c64Y - kC64DrawCoordinateYOffset);
    if (y >= kCanonicalCaveSize.height)
    {
        throw std::invalid_argument("A cave drawing command starts outside the logical cave.");
    }

    return {static_cast<std::ptrdiff_t>(x), static_cast<std::ptrdiff_t>(y)};
}

void setCell(CaveGrid &grid, const DrawVector position, const CellCode object)
{
    if (position.x < 0 || position.y < 0 ||
        !grid.contains({static_cast<std::size_t>(position.x), static_cast<std::size_t>(position.y)}))
    {
        throw std::invalid_argument("A cave drawing command writes outside the logical cave.");
    }

    grid.set({static_cast<std::size_t>(position.x), static_cast<std::size_t>(position.y)}, object);
}

void drawLine(CaveGrid &grid, DrawVector position, const CellCode object, const CellCode length,
              const CellCode direction)
{
    const DrawVector step = directionVector(direction);
    for (std::size_t index = 0; index < length; ++index)
    {
        setCell(grid, position, object);
        position.x += step.x;
        position.y += step.y;
    }
}

void drawFilledRectangle(CaveGrid &grid, const DrawVector origin, const CellCode object, const CellCode width,
                         const CellCode height)
{
    for (std::size_t y = 0; y < height; ++y)
    {
        for (std::size_t x = width; x > 0; --x)
        {
            setCell(grid, {origin.x + static_cast<std::ptrdiff_t>(x - 1), origin.y + static_cast<std::ptrdiff_t>(y)},
                    object);
        }
    }
}

void drawFilledRectangleWithInterior(CaveGrid &grid, const CaveDrawCommand &command)
{
    const DrawVector origin = projectDrawPosition(command.x, command.y);
    drawFilledRectangle(grid, origin, command.object, command.parameter3, command.parameter4);

    if (command.parameter3 <= 2 || command.parameter4 <= 2)
    {
        throw std::invalid_argument("A filled cave rectangle with an interior must be at least 3 by 3.");
    }

    drawFilledRectangle(grid, {origin.x + 1, origin.y + 1}, command.parameter5,
                        static_cast<CellCode>(command.parameter3 - 2), static_cast<CellCode>(command.parameter4 - 2));
}

void drawRectangle(CaveGrid &grid, const CaveDrawCommand &command)
{
    const DrawVector origin = projectDrawPosition(command.x, command.y);
    DrawVector cursor = origin;

    const auto drawEdge = [&grid, command](DrawVector &edgeCursor, const CellCode length, const CellCode direction) {
        const DrawVector step = directionVector(direction);
        for (std::size_t index = 0; index < length; ++index)
        {
            setCell(grid, edgeCursor, command.object);
            edgeCursor.x += step.x;
            edgeCursor.y += step.y;
        }
    };

    drawEdge(cursor, static_cast<CellCode>(command.parameter3 - 1), 2);
    drawEdge(cursor, static_cast<CellCode>(command.parameter4 - 1), 4);
    drawEdge(cursor, static_cast<CellCode>(command.parameter3 - 1), 6);
    drawEdge(cursor, static_cast<CellCode>(command.parameter4 - 1), 0);
}

void executeDrawCommand(CaveGrid &grid, CaveMetadata &metadata, const CaveDrawCommand &command)
{
    const DrawVector origin = projectDrawPosition(command.x, command.y);
    if (command.object == kInbox)
    {
        metadata.rockfordEntry = CellPosition{static_cast<std::size_t>(origin.x), static_cast<std::size_t>(origin.y)};
    }

    switch (command.kind)
    {
    case CaveDrawCommandKind::SingleObject:
        setCell(grid, origin, command.object);
        return;
    case CaveDrawCommandKind::Line:
        drawLine(grid, origin, command.object, command.parameter3, command.parameter4);
        return;
    case CaveDrawCommandKind::FilledRectangle:
        drawFilledRectangleWithInterior(grid, command);
        return;
    case CaveDrawCommandKind::Rectangle:
        if (command.parameter3 == 0 || command.parameter4 == 0)
        {
            throw std::invalid_argument("A cave rectangle must have non-zero dimensions.");
        }
        drawRectangle(grid, command);
        return;
    }

    throw std::logic_error("The cave drawing command kind is invalid.");
}

[[nodiscard]] CellCode chooseRandomObject(const CaveHeader &header, PseudoRandomState &random)
{
    const CellCode value = random.next();
    CellCode object = kDirt;

    for (std::size_t index = 0; index < kCaveRandomObjectCount; ++index)
    {
        if (value < header.randomObjectProbabilities[index])
        {
            object = header.randomObjects[index];
        }
    }

    return object;
}

void fillWithDirtAndRandomObjects(CaveGrid &grid, const CaveHeader &header, PseudoRandomState &random)
{
    // FillWithDirtAndRandomObjects at $7cab starts at CaveMatrixY = 1 and
    // stops before 22. The top row is subsequently supplied by the steel frame.
    for (std::size_t y = 1; y < kCanonicalCaveSize.height; ++y)
    {
        for (std::size_t x = 0; x < kCanonicalCaveSize.width; ++x)
        {
            grid.set({x, y}, chooseRandomObject(header, random));
        }
    }
}

void frameWithSteelWall(CaveGrid &grid)
{
    drawRectangle(grid, {CaveDrawCommandKind::Rectangle, kSteelWall, 0, 2, 40, 22, 0});
}

[[nodiscard]] CaveMetadata makeMetadata(const CaveHeader &header, const std::uint8_t sublevelIndex,
                                        const PseudoRandomState &random)
{
    return {
        header.caveNumber,
        sublevelIndex,
        {
            header.magicWallMillingTimeOrAmoeba3PercentMax,
            header.initialDiamondValue,
            header.extraDiamondValue,
            header.diamondsRequired[sublevelIndex],
            header.caveTimes[sublevelIndex],
            header.backgroundColour1,
            header.backgroundColour2,
            header.foregroundColour,
            header.reservedBytes,
        },
        std::nullopt,
        random.seed1,
        random.seed2,
    };
}

} // namespace

PreparedCave prepareCave(const CaveDefinition &definition, const std::uint8_t sublevelIndex)
{
    return prepareCaveWithTrace(definition, sublevelIndex).preparedCave;
}

CavePreparationTrace prepareCaveWithTrace(const CaveDefinition &definition, const std::uint8_t sublevelIndex)
{
    if (sublevelIndex >= kCaveSublevelCount)
    {
        throw std::invalid_argument("The cave sublevel index is outside the C64 range.");
    }

    PseudoRandomState random{0, definition.header.initialRandomSeeds[sublevelIndex]};
    CaveMetadata metadata = makeMetadata(definition.header, sublevelIndex, random);
    CaveGrid grid(kCanonicalCaveSize);

    fillWithDirtAndRandomObjects(grid, definition.header, random);
    CaveGrid afterRandomFill = grid;
    frameWithSteelWall(grid);
    CaveGrid afterSteelFrame = grid;
    for (const CaveDrawCommand &command : definition.drawCommands)
    {
        executeDrawCommand(grid, metadata, command);
    }

    metadata.randomSeed1 = random.seed1;
    metadata.randomSeed2 = random.seed2;
    return {std::move(afterRandomFill), std::move(afterSteelFrame), {std::move(grid), std::move(metadata)}};
}

GameState makeInitialGameState(const PreparedCave &preparedCave)
{
    CaveRuntimeState runtime;
    runtime.rockfordPosition = preparedCave.metadata.rockfordEntry;
    runtime.timeBasedRandom.sample = {0x2d, 0x44, 0x8f, 0x9a, 0x6c, 0x1b};
    // InitGameVariablesFromLevelData sets FlashingEntryBoxCountDown to four
    // seconds for the ordinary one-player Cave A path ($7f67-$7f6f).
    runtime.appearanceCountdown = 4;
    return {preparedCave.grid,     0,      SessionPhase::RockfordAppearing, {Direction::Neutral},
            preparedCave.metadata, runtime};
}

} // namespace boulderdash::engine
