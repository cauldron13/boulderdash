#include "engine/CaveGrid.h"

#include <limits>
#include <stdexcept>

namespace boulderdash::engine
{

CaveGrid::CaveGrid(const CaveSize size, const CellCode initialCell) : size_(size)
{
    if (size.width == 0 || size.height == 0)
    {
        throw std::invalid_argument("A cave grid must have non-zero dimensions.");
    }

    if (size.height > std::numeric_limits<std::size_t>::max() / size.width)
    {
        throw std::length_error("The cave grid dimensions overflow the cell container.");
    }

    cells_.assign(size.width * size.height, initialCell);
}

CaveSize CaveGrid::size() const noexcept
{
    return size_;
}

bool CaveGrid::contains(const CellPosition position) const noexcept
{
    return position.x < size_.width && position.y < size_.height;
}

const CellCode &CaveGrid::at(const CellPosition position) const
{
    return cells_.at(indexOf(position));
}

void CaveGrid::set(const CellPosition position, const CellCode cell)
{
    CellCode &existingCell = cells_.at(indexOf(position));
    if (existingCell == cell)
    {
        return;
    }

    existingCell = cell;
    ++revision_;
}

std::uint64_t CaveGrid::revision() const noexcept
{
    return revision_;
}

std::size_t CaveGrid::indexOf(const CellPosition position) const
{
    if (!contains(position))
    {
        throw std::out_of_range("The cave cell position is outside the grid.");
    }

    return position.y * size_.width + position.x;
}

} // namespace boulderdash::engine
