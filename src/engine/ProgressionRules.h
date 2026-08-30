#pragma once

#include "engine/GameState.h"

#include <cstdint>

namespace boulderdash::engine
{

constexpr std::uint64_t kC64PalCaveCompletionScoreLoopCycles = 26968;
constexpr std::uint64_t kC64PalCaveCompletionPostScoreDelayCycles = 1158537;
constexpr std::uint64_t kC64PalCaveTransitionCycles = 2808978;

void initializeCaveProgress(GameState &state);
void collectDiamond(GameState &state);
void advanceActiveCaveSecond(GameState &state);
void advanceCompletedCaveCycles(GameState &state, std::uint64_t cycles);
void advanceRockfordDeath(GameState &state);

} // namespace boulderdash::engine
