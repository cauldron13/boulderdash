#pragma once

#include "engine/CaveUpdateContext.h"

namespace boulderdash::engine
{

void processRockfordAppearance(CaveUpdateContext &context, CellPosition position);
void processInAndOutBoxes(CaveUpdateContext &context, CellPosition position);
void processRockford(CaveUpdateContext &context, CellPosition position);

} // namespace boulderdash::engine
