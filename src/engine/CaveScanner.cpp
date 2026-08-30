#include "engine/CaveScanner.h"

#include "engine/AmoebaRules.h"
#include "engine/CaveUpdateContext.h"
#include "engine/EnemyRules.h"
#include "engine/FallingObjectRules.h"
#include "engine/ObjectCodes.h"
#include "engine/RockfordRules.h"

#include <array>
#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

constexpr std::size_t kC64CaveWidth = 40;
constexpr std::size_t kFirstScannedRow = 1;
constexpr std::size_t kNormalScanEndExclusive = 22;
constexpr std::size_t kBonusScanEndExclusive = 15;
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

CaveScanControl dispatchCell(CaveUpdateContext *context, CaveGrid &grid, const CellPosition position)
{
    if (context == nullptr)
    {
        return CaveScanControl::Continue;
    }

    switch (objectcodes::objectHandlerForCode(grid.at(position)))
    {
    case objectcodes::ObjectHandler::Amoeba:
        processAmoeba(*context, position);
        break;
    case objectcodes::ObjectHandler::Firefly:
        processFirefly(*context, position);
        break;
    case objectcodes::ObjectHandler::Butterfly:
        processButterfly(*context, position);
        break;
    case objectcodes::ObjectHandler::Explosion:
        processExplosion(*context, position);
        break;
    case objectcodes::ObjectHandler::StationaryBoulder:
        processStationaryBoulder(*context, position);
        break;
    case objectcodes::ObjectHandler::FallingBoulder:
        return processFallingBoulder(*context, position);
    case objectcodes::ObjectHandler::StationaryDiamond:
        processStationaryDiamond(*context, position);
        break;
    case objectcodes::ObjectHandler::FallingDiamond:
        processFallingDiamond(*context, position);
        break;
    case objectcodes::ObjectHandler::HiddenOutbox:
        if (context->state.runtime.has_value() && context->state.runtime->exitOpen)
        {
            grid.set(position, objectcodes::kOpenOutbox);
        }
        break;
    case objectcodes::ObjectHandler::InAndOutBoxes:
        processInAndOutBoxes(*context, position);
        break;
    case objectcodes::ObjectHandler::RockfordAppearance:
        processRockfordAppearance(*context, position);
        break;
    case objectcodes::ObjectHandler::Rockford:
        processRockford(*context, position);
        break;
    case objectcodes::ObjectHandler::None:
        break;
    }

    return CaveScanControl::Continue;
}

void normalizeScanTrailer(CaveGrid &grid, const CellPosition currentPosition)
{
    // __ProcessCave__TurnTrailerToBaseType ($7e15-$7e1f) updates the NW
    // trailer cell, not the cell currently being dispatched.
    const std::size_t currentLinearPosition = currentPosition.y * kC64CaveWidth + currentPosition.x;
    constexpr std::size_t kC64ScanTrailerDistance = kC64CaveWidth + 1;
    if (currentLinearPosition < kC64ScanTrailerDistance)
    {
        return;
    }

    const std::size_t trailerLinearPosition = currentLinearPosition - kC64ScanTrailerDistance;
    const CellPosition trailer{trailerLinearPosition % kC64CaveWidth, trailerLinearPosition / kC64CaveWidth};
    const CellCode normalized = objectcodes::normalizeScannedCode(grid.at(trailer));
    if (normalized != objectcodes::kEmpty)
    {
        grid.set(trailer, normalized);
    }
}

} // namespace

CaveScanSummary CaveScanner::scan(CaveGrid &grid, const CaveScanKind kind) const
{
    CaveUpdateContext *context = nullptr;
    const CaveSize size = grid.size();
    const std::size_t endExclusive = kind == CaveScanKind::Bonus ? kBonusScanEndExclusive : kNormalScanEndExclusive;
    if (size.width != kC64CaveWidth || size.height < endExclusive)
    {
        throw std::invalid_argument("The cave grid cannot represent the requested C64 scan area.");
    }

    CaveScanSummary summary;
    summary.visitDigest = kFnvOffsetBasis;
    for (std::size_t y = kFirstScannedRow; y < endExclusive; ++y)
    {
        for (std::size_t x = 0; x < kC64CaveWidth; ++x)
        {
            const CellPosition position{x, y};
            if (summary.visitedCellCount == 0)
            {
                summary.firstVisited = position;
            }
            const CaveScanControl control = dispatchCell(context, grid, position);
            if (control != CaveScanControl::Abort)
            {
                normalizeScanTrailer(grid, position);
            }
            const std::uint64_t linearPosition = static_cast<std::uint64_t>(position.y * kC64CaveWidth + position.x);
            summary.visitDigest ^= linearPosition;
            summary.visitDigest *= kFnvPrime;
            summary.lastVisited = position;
            ++summary.visitedCellCount;
            if (control == CaveScanControl::Abort)
            {
                return summary;
            }
        }
    }

    return summary;
}

CaveScanSummary CaveScanner::scan(CaveUpdateContext &context, const CaveScanKind kind) const
{
    CaveGrid &grid = context.state.grid;
    const CaveSize size = grid.size();
    const std::size_t endExclusive = kind == CaveScanKind::Bonus ? kBonusScanEndExclusive : kNormalScanEndExclusive;
    if (size.width != kC64CaveWidth || size.height < endExclusive)
    {
        throw std::invalid_argument("The cave grid cannot represent the requested C64 scan area.");
    }

    CaveScanSummary summary;
    summary.visitDigest = kFnvOffsetBasis;
    for (std::size_t y = kFirstScannedRow; y < endExclusive; ++y)
    {
        for (std::size_t x = 0; x < kC64CaveWidth; ++x)
        {
            const CellPosition position{x, y};
            if (summary.visitedCellCount == 0)
            {
                summary.firstVisited = position;
            }
            const CaveScanControl control = dispatchCell(&context, grid, position);
            if (control != CaveScanControl::Abort)
            {
                normalizeScanTrailer(grid, position);
            }
            const std::uint64_t linearPosition = static_cast<std::uint64_t>(position.y * kC64CaveWidth + position.x);
            summary.visitDigest ^= linearPosition;
            summary.visitDigest *= kFnvPrime;
            summary.lastVisited = position;
            ++summary.visitedCellCount;
            if (control == CaveScanControl::Abort)
            {
                return summary;
            }
        }
    }

    return summary;
}

} // namespace boulderdash::engine
