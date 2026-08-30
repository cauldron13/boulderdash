#pragma once

#include "engine/CaveDefinition.h"
#include "engine/CaveGrid.h"
#include "engine/GameState.h"

namespace boulderdash::engine
{

struct PreparedCave final
{
    CaveGrid grid;
    CaveMetadata metadata;
};

struct CavePreparationTrace final
{
    CaveGrid afterRandomFill;
    CaveGrid afterSteelFrame;
    PreparedCave preparedCave;
};

[[nodiscard]] PreparedCave prepareCave(const CaveDefinition &definition, std::uint8_t sublevelIndex);
[[nodiscard]] CavePreparationTrace prepareCaveWithTrace(const CaveDefinition &definition, std::uint8_t sublevelIndex);
[[nodiscard]] GameState makeInitialGameState(const PreparedCave &preparedCave);

} // namespace boulderdash::engine
