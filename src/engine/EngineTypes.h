#pragma once

#include <cstddef>
#include <cstdint>

namespace boulderdash::engine
{

using CellCode = std::uint8_t;
using LogicalTick = std::uint32_t;

struct CellPosition final
{
    std::size_t x = 0;
    std::size_t y = 0;
};

inline bool operator==(const CellPosition &left, const CellPosition &right)
{
    return left.x == right.x && left.y == right.y;
}

inline bool operator!=(const CellPosition &left, const CellPosition &right)
{
    return !(left == right);
}

struct CaveSize final
{
    std::size_t width = 0;
    std::size_t height = 0;
};

inline bool operator==(const CaveSize &left, const CaveSize &right)
{
    return left.width == right.width && left.height == right.height;
}

inline bool operator!=(const CaveSize &left, const CaveSize &right)
{
    return !(left == right);
}

enum class Direction : std::uint8_t
{
    Neutral,
    North,
    NorthEast,
    East,
    SouthEast,
    South,
    SouthWest,
    West,
    NorthWest,
};

struct GameCommand final
{
    Direction direction = Direction::Neutral;
    bool firePressed = false;
};

inline bool operator==(const GameCommand &left, const GameCommand &right)
{
    return left.direction == right.direction && left.firePressed == right.firePressed;
}

inline bool operator!=(const GameCommand &left, const GameCommand &right)
{
    return !(left == right);
}

// This vocabulary is provisional until each C64 transition has been analysed.
enum class SessionPhase : std::uint8_t
{
    Uninitialized,
    CavePrepared,
    RockfordAppearing,
    Playing,
    CaveCompleted,
    CaveTransitioning,
    RockfordDead,
    Transitioning,
    GameOver,
};

enum class GameEventType : std::uint8_t
{
    DugDirt,
    RockfordMovedThroughEmptySpace,
    DiamondCollected,
    DiamondQuotaReached,
    ExitEntered,
    BoulderPushed,
    BoulderImpact,
    DiamondFalling,
    Explosion,
    RockfordDied,
    MagicWallActivated,
    RockfordAppearanceStarted,
    TimeRunningOut,
};

struct GameEvent final
{
    GameEventType type = GameEventType::DugDirt;
    CellPosition position;
    // Event-specific one-based sound variant or remaining-second value; zero means no payload.
    std::uint8_t value = 0;
};

inline bool operator==(const GameEvent &left, const GameEvent &right)
{
    return left.type == right.type && left.position == right.position && left.value == right.value;
}

inline bool operator!=(const GameEvent &left, const GameEvent &right)
{
    return !(left == right);
}

} // namespace boulderdash::engine
