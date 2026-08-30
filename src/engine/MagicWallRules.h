#pragma once

#include "engine/CaveUpdateContext.h"

namespace boulderdash::engine
{

[[nodiscard]] bool processFallingObjectAtMagicWall(CaveUpdateContext &context, CellPosition position,
                                                   CellCode scannedConvertedObject);
void advanceMagicWallFrame(GameState &state);

} // namespace boulderdash::engine
