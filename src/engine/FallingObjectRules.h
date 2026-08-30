#pragma once

#include "engine/CaveUpdateContext.h"

namespace boulderdash::engine
{

void processStationaryBoulder(CaveUpdateContext &context, CellPosition position);
[[nodiscard]] CaveScanControl processFallingBoulder(CaveUpdateContext &context, CellPosition position);
void processStationaryDiamond(CaveUpdateContext &context, CellPosition position);
void processFallingDiamond(CaveUpdateContext &context, CellPosition position);

} // namespace boulderdash::engine
