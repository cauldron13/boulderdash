#pragma once

#include "engine/CaveUpdateContext.h"

namespace boulderdash::engine
{

void processFirefly(CaveUpdateContext &context, CellPosition position);
void processButterfly(CaveUpdateContext &context, CellPosition position);
void processExplosion(CaveUpdateContext &context, CellPosition position);
void explodeFallingObject(CaveUpdateContext &context, CellPosition position, bool producesDiamonds);

} // namespace boulderdash::engine
