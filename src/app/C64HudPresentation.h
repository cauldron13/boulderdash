#pragma once

#include "engine/GameState.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace boulderdash::app
{

constexpr std::size_t kC64HudCharacterColumns = 20;
constexpr std::size_t kC64HudScreenColumns = kC64HudCharacterColumns * 2;

using C64HudColourRamLine = std::array<std::uint8_t, kC64HudScreenColumns>;

enum class C64HudPresentationKind : std::uint8_t
{
    Scoreboard,
    CaveCompletion,
    InterCave,
    Loading,
    GameOver,
};

struct C64HudPresentation final
{
    C64HudPresentationKind kind = C64HudPresentationKind::Scoreboard;
    std::array<engine::CellCode, kC64HudCharacterColumns> scoreLine{};
    C64HudColourRamLine scoreLineColourRam{};
    std::array<engine::CellCode, kC64HudCharacterColumns> playerLine{};
    C64HudColourRamLine playerLineColourRam{};
};

[[nodiscard]] C64HudPresentation c64HudPresentationForSnapshot(const engine::GameSnapshot &snapshot);

} // namespace boulderdash::app
