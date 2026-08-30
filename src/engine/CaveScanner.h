#pragma once

#include "engine/CaveGrid.h"

#include <cstdint>

namespace boulderdash::engine
{

struct CaveUpdateContext;

enum class CaveScanKind : std::uint8_t
{
    Normal,
    Bonus,
};

struct CaveScanSummary final
{
    std::size_t visitedCellCount = 0;
    CellPosition firstVisited;
    CellPosition lastVisited;
    std::uint64_t visitDigest = 0;
};

class CaveScanner final
{
  public:
    [[nodiscard]] CaveScanSummary scan(CaveGrid &grid, CaveScanKind kind) const;
    [[nodiscard]] CaveScanSummary scan(CaveUpdateContext &context, CaveScanKind kind) const;
};

} // namespace boulderdash::engine
