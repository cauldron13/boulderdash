#pragma once

#include "engine/EngineTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace boulderdash::engine
{

class CaveGrid final
{
  public:
    explicit CaveGrid(CaveSize size, CellCode initialCell = 0);

    [[nodiscard]] CaveSize size() const noexcept;
    [[nodiscard]] bool contains(CellPosition position) const noexcept;

    [[nodiscard]] const CellCode &at(CellPosition position) const;
    void set(CellPosition position, CellCode cell);
    [[nodiscard]] std::uint64_t revision() const noexcept;

  private:
    [[nodiscard]] std::size_t indexOf(CellPosition position) const;

    CaveSize size_;
    std::vector<CellCode> cells_;
    std::uint64_t revision_ = 0;
};

} // namespace boulderdash::engine
