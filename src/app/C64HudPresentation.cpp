#include "app/C64HudPresentation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace boulderdash::app
{
namespace
{

constexpr engine::CellCode kC64SpaceCharacter = 0x20;
constexpr engine::CellCode kC64DigitZeroCharacter = 0x10;
constexpr engine::CellCode kC64LetterACharacter = 0x21;
constexpr engine::CellCode kC64DiamondCharacter = 0x3c;
constexpr engine::CellCode kC64PlayerCharacterIndex = 7;
constexpr engine::CellCode kC64LivesCharacterIndex = 10;
constexpr engine::CellCode kC64CaveCharacterIndex = 16;
constexpr engine::CellCode kC64LevelCharacterIndex = 18;
constexpr std::uint8_t kC64WhiteMulticolourColourRamValue = 0x09;
constexpr std::uint8_t kC64YellowHighResolutionColourRamValue = 0x07;
constexpr std::uint32_t kC64ScoreModulo = 1'000'000;
constexpr std::uint16_t kC64TimeDisplayMaximum = 999;
constexpr std::uint8_t kC64LivesDisplayMaximum = 9;
constexpr std::uint8_t kC64PlayerDisplayMaximum = 2;
constexpr std::uint8_t kC64SublevelDisplayMaximum = 5;

constexpr std::array<engine::CellCode, kC64HudCharacterColumns> kC64PreLevelMarqueeText = {
    0x30, 0x2c, 0x21, 0x39, 0x25, 0x32, 0x20, 0x11, 0x0d, 0x20,
    0x13, 0x20, 0x2d, 0x25, 0x2e, 0x20, 0x21, 0x0f, 0x10, 0x20,
};

constexpr std::array<engine::CellCode, kC64HudCharacterColumns> kC64GameOverText = {
    0x20, 0x27, 0x20, 0x21, 0x20, 0x2d, 0x20, 0x25, 0x20, 0x20,
    0x20, 0x2f, 0x20, 0x36, 0x20, 0x25, 0x20, 0x32, 0x20, 0x20,
};

[[nodiscard]] C64HudColourRamLine colourRamForTopLine(
    const std::array<engine::CellCode, kC64HudCharacterColumns> &characters)
{
    C64HudColourRamLine colourRam{};
    colourRam.fill(kC64WhiteMulticolourColourRamValue);

    // SetTopLineText at $6b16 determines these colours from the text buffer.
    // Color RAM has one entry per physical C64 character, while the text buffer
    // stores one entry per two-character-wide glyph.
    if (characters[3] == kC64DiamondCharacter)
    {
        std::fill_n(colourRam.begin() + 14, 4, kC64YellowHighResolutionColourRamValue);
    }
    if (characters[1] < 0x19)
    {
        std::fill_n(colourRam.begin() + 2, 4, kC64YellowHighResolutionColourRamValue);
    }

    return colourRam;
}

[[nodiscard]] C64HudPresentationKind presentationKindForPhase(const engine::SessionPhase phase)
{
    switch (phase)
    {
    case engine::SessionPhase::CaveCompleted:
        return C64HudPresentationKind::CaveCompletion;
    case engine::SessionPhase::CaveTransitioning:
        return C64HudPresentationKind::InterCave;
    case engine::SessionPhase::Transitioning:
        return C64HudPresentationKind::Loading;
    case engine::SessionPhase::GameOver:
        return C64HudPresentationKind::GameOver;
    case engine::SessionPhase::Uninitialized:
    case engine::SessionPhase::CavePrepared:
    case engine::SessionPhase::RockfordAppearing:
    case engine::SessionPhase::Playing:
    case engine::SessionPhase::RockfordDead:
        return C64HudPresentationKind::Scoreboard;
    }

    throw std::logic_error("The game snapshot contains an invalid session phase.");
}

[[nodiscard]] std::array<engine::CellCode, kC64HudCharacterColumns> scoreLineForSnapshot(
    const engine::GameSnapshot &snapshot)
{
    std::array<engine::CellCode, kC64HudCharacterColumns> characters{};
    characters.fill(kC64SpaceCharacter);

    const std::uint8_t playerIndex = snapshot.campaign.currentPlayer;
    const std::uint32_t score = playerIndex < snapshot.campaign.players.size()
                                    ? snapshot.campaign.players[playerIndex].score % kC64ScoreModulo
                                    : 0;
    const std::uint16_t time =
        snapshot.runtime.has_value() ? std::min(snapshot.runtime->timeRemainingSeconds, kC64TimeDisplayMaximum) : 0;
    const std::uint16_t diamondQuota =
        snapshot.cave.has_value() ? std::min<std::uint16_t>(snapshot.cave->configuration.diamondsRequired, 99) : 0;
    const bool diamondQuotaReached = snapshot.runtime.has_value() && snapshot.runtime->exitOpen;
    const std::uint8_t diamondValue =
        snapshot.cave.has_value()
            ? std::min<std::uint8_t>(diamondQuotaReached ? snapshot.cave->configuration.extraDiamondValue
                                                         : snapshot.cave->configuration.initialDiamondValue,
                                     99)
            : 0;
    const std::uint16_t collectedDiamonds =
        snapshot.runtime.has_value() ? std::min<std::uint16_t>(snapshot.runtime->collectedDiamonds, 99) : 0;

    const auto writeDigits = [&characters](const std::size_t start, const std::uint32_t value,
                                           const std::size_t digitCount) {
        std::uint32_t divisor = 1;
        for (std::size_t index = 1; index < digitCount; ++index)
        {
            divisor *= 10;
        }
        for (std::size_t index = 0; index < digitCount; ++index)
        {
            characters[start + index] = static_cast<engine::CellCode>(kC64DigitZeroCharacter + value / divisor % 10U);
            divisor /= 10;
        }
    };

    // CurrentPlayerScoresText at $983a is displayed by SetTopLineToScore.
    // Its observed Cave A/1 layout is " 12<diamond>10 00 150 000000".
    if (diamondQuotaReached)
    {
        // CheckIfGotDiamondQuota ($717a-$717f) replaces both quota digits with diamonds.
        characters[1] = kC64DiamondCharacter;
        characters[2] = kC64DiamondCharacter;
    }
    else
    {
        writeDigits(1, diamondQuota, 2);
    }
    characters[3] = kC64DiamondCharacter;
    writeDigits(4, diamondValue, 2);
    writeDigits(7, collectedDiamonds, 2);
    writeDigits(10, time, 3);
    writeDigits(14, score, 6);
    return characters;
}

[[nodiscard]] std::array<engine::CellCode, kC64HudCharacterColumns> playerLineForSnapshot(
    const engine::GameSnapshot &snapshot, const bool showPendingCave)
{
    std::array<engine::CellCode, kC64HudCharacterColumns> characters = kC64PreLevelMarqueeText;
    const std::uint8_t playerIndex = snapshot.campaign.currentPlayer;
    const std::uint8_t playerNumber =
        std::min<std::uint8_t>(static_cast<std::uint8_t>(playerIndex + 1U), kC64PlayerDisplayMaximum);
    const std::uint8_t lives = playerIndex < snapshot.campaign.players.size()
                                   ? std::min(snapshot.campaign.players[playerIndex].lives, kC64LivesDisplayMaximum)
                                   : 0;
    const engine::CellCode caveNumber = showPendingCave && snapshot.campaign.pendingCaveNumber != 0
                                            ? snapshot.campaign.pendingCaveNumber
                                        : snapshot.cave.has_value() ? snapshot.cave->caveNumber
                                                                    : 0;
    const std::uint8_t levelNumber = std::min<std::uint8_t>(
        static_cast<std::uint8_t>(snapshot.campaign.sublevelIndex + 1U), kC64SublevelDisplayMaximum);

    characters[kC64PlayerCharacterIndex] = static_cast<engine::CellCode>(kC64DigitZeroCharacter + playerNumber);
    characters[kC64LivesCharacterIndex] = static_cast<engine::CellCode>(kC64DigitZeroCharacter + lives);
    characters[kC64CaveCharacterIndex] =
        caveNumber == 0 ? kC64SpaceCharacter : static_cast<engine::CellCode>(kC64LetterACharacter + caveNumber - 1U);
    characters[kC64LevelCharacterIndex] = static_cast<engine::CellCode>(kC64DigitZeroCharacter + levelNumber);
    return characters;
}

} // namespace

C64HudPresentation c64HudPresentationForSnapshot(const engine::GameSnapshot &snapshot)
{
    const C64HudPresentationKind kind = presentationKindForPhase(snapshot.phase);
    if (kind == C64HudPresentationKind::GameOver)
    {
        std::array<engine::CellCode, kC64HudCharacterColumns> blankLine{};
        blankLine.fill(kC64SpaceCharacter);
        return {kind, kC64GameOverText, colourRamForTopLine(kC64GameOverText), blankLine,
                colourRamForTopLine(blankLine)};
    }

    const bool showPendingCave = kind == C64HudPresentationKind::InterCave || kind == C64HudPresentationKind::Loading;
    const std::array<engine::CellCode, kC64HudCharacterColumns> scoreLine = scoreLineForSnapshot(snapshot);
    const std::array<engine::CellCode, kC64HudCharacterColumns> playerLine =
        playerLineForSnapshot(snapshot, showPendingCave);
    return {kind, scoreLine, colourRamForTopLine(scoreLine), playerLine, colourRamForTopLine(playerLine)};
}

} // namespace boulderdash::app
