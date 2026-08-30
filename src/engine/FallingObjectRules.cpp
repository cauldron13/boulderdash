#include "engine/FallingObjectRules.h"

#include "engine/EnemyRules.h"
#include "engine/MagicWallRules.h"
#include "engine/ObjectCodes.h"

#include <stdexcept>

namespace boulderdash::engine
{
namespace
{

bool isSlippery(const CellCode code)
{
    return code == objectcodes::kStationaryBoulder || code == objectcodes::kStationaryDiamond ||
           code == objectcodes::kBrickWall;
}

bool isEmpty(const CaveGrid &grid, const CellPosition position)
{
    return grid.contains(position) && grid.at(position) == objectcodes::kEmpty;
}

void emitFallingSoundEvent(CaveUpdateContext &context, const GameEventType type, const CellPosition position,
                           const bool inputCarry)
{
    std::uint8_t variant = 0;
    if (type == GameEventType::DiamondFalling)
    {
        if (!context.state.runtime.has_value())
        {
            throw std::logic_error("A falling-diamond sound requires a cave runtime state.");
        }

        // FallingDiamondSoundFX ($6e28-$6e4e) consumes one random value for the low frequency byte and
        // selects one of eight high frequency variants with the second value. EOR preserves the ADC carry.
        TimeBasedRandomState &random = context.state.runtime->timeBasedRandom;
        const TimeBasedRandomResult lowFrequency = random.nextWithCarry(inputCarry);
        const TimeBasedRandomResult highFrequency = random.nextWithCarry(lowFrequency.carry);
        variant = static_cast<std::uint8_t>((highFrequency.value & 0x07U) + 1U);
    }
    emitGameEvent(context, type, position, variant);
}

CaveScanControl processObject(CaveUpdateContext &context, const CellPosition position, const CellCode falling,
                              const CellCode scannedStationary, const CellCode scannedFalling,
                              const bool abortScanAfterMagicWall)
{
    CaveGrid &grid = context.state.grid;
    const CaveSize size = grid.size();
    if (position.y + 1 >= size.height)
    {
        return CaveScanControl::Continue;
    }

    const CellPosition below{position.x, position.y + 1};
    const bool wasFalling = grid.at(position) == falling;
    const bool isBoulder = falling == objectcodes::kFallingBoulder;
    const GameEventType contactEvent = isBoulder ? GameEventType::BoulderImpact : GameEventType::DiamondFalling;
    if (isEmpty(grid, below))
    {
        grid.set(below, scannedFalling);
        grid.set(position, objectcodes::kEmpty);
        if (!wasFalling && !isBoulder)
        {
            emitFallingSoundEvent(context, contactEvent, position, false);
        }
        return CaveScanControl::Continue;
    }

    const CellCode belowCode = grid.at(below);
    if (wasFalling &&
        processFallingObjectAtMagicWall(context, position,
                                        falling == objectcodes::kFallingBoulder ? objectcodes::kScannedFallingDiamond
                                                                                : objectcodes::kScannedFallingBoulder))
    {
        emitFallingSoundEvent(context, isBoulder ? GameEventType::DiamondFalling : GameEventType::BoulderImpact,
                              position, false);
        return abortScanAfterMagicWall ? CaveScanControl::Abort : CaveScanControl::Continue;
    }
    // ProcessFallingDiamond ($6f6b) and ProcessFallingBoulder ($741a) compare
    // exactly against unscanned Rockford ($38), not the scan marker ($39).
    if (wasFalling && belowCode == objectcodes::kRockford)
    {
        emitFallingSoundEvent(context, contactEvent, position, belowCode >= objectcodes::kMagicWall);
        explodeFallingObject(context, position, false);
        return CaveScanControl::Continue;
    }
    // CheckIfFallingObjectKillsAnimal ($6e7c-$6eaa) rejects enemy markers
    // created during the current scan ($34-$37 and $0c-$0f).
    if (wasFalling && belowCode >= 0x30 && belowCode <= 0x33)
    {
        emitFallingSoundEvent(context, contactEvent, position, belowCode >= objectcodes::kMagicWall);
        explodeFallingObject(context, position, true);
        return CaveScanControl::Continue;
    }
    if (wasFalling && belowCode >= objectcodes::kFireflyLeft && belowCode <= objectcodes::kFireflyDown)
    {
        emitFallingSoundEvent(context, contactEvent, position, belowCode >= objectcodes::kMagicWall);
        explodeFallingObject(context, position, false);
        return CaveScanControl::Continue;
    }

    if (isSlippery(belowCode))
    {
        // A rolling boulder remains in the same fall episode, so the sample-based port waits for its final contact.
        // Falling diamonds retain the C64 tinkle when they change trajectory.
        if (position.x > 0)
        {
            const CellPosition left{position.x - 1, position.y};
            const CellPosition downLeft{position.x - 1, position.y + 1};
            if (isEmpty(grid, left) && isEmpty(grid, downLeft))
            {
                grid.set(left, scannedFalling);
                grid.set(position, objectcodes::kEmpty);
                if (wasFalling && !isBoulder)
                {
                    emitFallingSoundEvent(context, contactEvent, position, belowCode >= objectcodes::kMagicWall);
                }
                return CaveScanControl::Continue;
            }
        }
        if (position.x + 1 < size.width)
        {
            const CellPosition right{position.x + 1, position.y};
            const CellPosition downRight{position.x + 1, position.y + 1};
            if (isEmpty(grid, right) && isEmpty(grid, downRight))
            {
                grid.set(right, scannedFalling);
                grid.set(position, objectcodes::kEmpty);
                if (wasFalling && !isBoulder)
                {
                    emitFallingSoundEvent(context, contactEvent, position, belowCode >= objectcodes::kMagicWall);
                }
                return CaveScanControl::Continue;
            }
        }
    }

    if (wasFalling)
    {
        grid.set(position, scannedStationary);
        emitFallingSoundEvent(context, contactEvent, position, belowCode >= objectcodes::kMagicWall);
    }

    return CaveScanControl::Continue;
}

} // namespace

void processStationaryBoulder(CaveUpdateContext &context, const CellPosition position)
{
    static_cast<void>(processObject(context, position, objectcodes::kFallingBoulder,
                                    objectcodes::kScannedStationaryBoulder, objectcodes::kScannedFallingBoulder,
                                    false));
}

CaveScanControl processFallingBoulder(CaveUpdateContext &context, const CellPosition position)
{
    return processObject(context, position, objectcodes::kFallingBoulder, objectcodes::kScannedStationaryBoulder,
                         objectcodes::kScannedFallingBoulder, true);
}

void processStationaryDiamond(CaveUpdateContext &context, const CellPosition position)
{
    static_cast<void>(processObject(context, position, objectcodes::kFallingDiamond,
                                    objectcodes::kScannedStationaryDiamond, objectcodes::kScannedFallingDiamond,
                                    false));
}

void processFallingDiamond(CaveUpdateContext &context, const CellPosition position)
{
    static_cast<void>(processObject(context, position, objectcodes::kFallingDiamond,
                                    objectcodes::kScannedStationaryDiamond, objectcodes::kScannedFallingDiamond,
                                    false));
}

} // namespace boulderdash::engine
