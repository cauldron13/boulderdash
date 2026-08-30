#pragma once

#include "engine/CaveUpdateContext.h"

namespace boulderdash::engine
{

void prepareAmoebaTick(GameState &state);
void processAmoeba(CaveUpdateContext &context, CellPosition position);
void advanceAmoebaSecond(GameState &state);

} // namespace boulderdash::engine
