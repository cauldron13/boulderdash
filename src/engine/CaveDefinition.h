#pragma once

#include "engine/EngineTypes.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace boulderdash::engine
{

inline constexpr std::size_t kCaveSublevelCount = 5;
inline constexpr std::size_t kCaveRandomObjectCount = 4;

enum class CaveDrawCommandKind : std::uint8_t
{
    SingleObject,
    Line,
    FilledRectangle,
    Rectangle,
};

struct CaveHeader final
{
    CellCode caveNumber = 0;
    CellCode magicWallMillingTimeOrAmoeba3PercentMax = 0;
    CellCode initialDiamondValue = 0;
    CellCode extraDiamondValue = 0;
    std::array<CellCode, kCaveSublevelCount> initialRandomSeeds{};
    std::array<CellCode, kCaveSublevelCount> diamondsRequired{};
    std::array<CellCode, kCaveSublevelCount> caveTimes{};
    CellCode backgroundColour1 = 0;
    CellCode backgroundColour2 = 0;
    CellCode foregroundColour = 0;
    std::array<CellCode, 2> reservedBytes{};
    std::array<CellCode, kCaveRandomObjectCount> randomObjects{};
    std::array<CellCode, kCaveRandomObjectCount> randomObjectProbabilities{};
};

struct CaveDrawCommand final
{
    CaveDrawCommandKind kind = CaveDrawCommandKind::SingleObject;
    CellCode object = 0;
    CellCode x = 0;
    CellCode y = 0;
    CellCode parameter3 = 0;
    CellCode parameter4 = 0;
    CellCode parameter5 = 0;
};

struct CaveDefinition final
{
    CaveHeader header;
    std::vector<CaveDrawCommand> drawCommands;
};

struct CaveConfiguration final
{
    CellCode magicWallMillingTimeOrAmoeba3PercentMax = 0;
    CellCode initialDiamondValue = 0;
    CellCode extraDiamondValue = 0;
    CellCode diamondsRequired = 0;
    CellCode caveTime = 0;
    CellCode backgroundColour1 = 0;
    CellCode backgroundColour2 = 0;
    CellCode foregroundColour = 0;
    std::array<CellCode, 2> reservedBytes{};
};

struct CaveMetadata final
{
    CellCode caveNumber = 0;
    std::uint8_t sublevelIndex = 0;
    CaveConfiguration configuration;
    std::optional<CellPosition> rockfordEntry;
    CellCode randomSeed1 = 0;
    CellCode randomSeed2 = 0;
};

[[nodiscard]] CaveDefinition decodeCaveDefinition(const std::vector<CellCode> &encoded);
[[nodiscard]] const CaveDefinition &caveA();
[[nodiscard]] const CaveDefinition &caveDefinition(CellCode caveNumber);

} // namespace boulderdash::engine
