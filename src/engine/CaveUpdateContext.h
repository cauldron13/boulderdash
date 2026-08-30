#pragma once

#include "engine/EngineTypes.h"
#include "engine/GameState.h"

#include <vector>

namespace boulderdash::engine
{

enum class CaveScanControl : std::uint8_t
{
    Continue,
    Abort,
};

struct CaveUpdateContext final
{
    GameState &state;
    GameCommand command;
    std::vector<GameEvent> *events = nullptr;
};

inline void emitGameEvent(CaveUpdateContext &context, const GameEventType type, const CellPosition position,
                          const std::uint8_t value = 0)
{
    if (context.events != nullptr)
    {
        context.events->push_back({type, position, value});
    }
}

} // namespace boulderdash::engine
