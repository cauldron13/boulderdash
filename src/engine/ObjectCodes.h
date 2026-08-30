#pragma once

#include "engine/EngineTypes.h"

#include <array>
#include <cstddef>

namespace boulderdash::engine::objectcodes
{

enum class ObjectHandler : std::uint8_t
{
    None,
    HiddenOutbox,
    InAndOutBoxes,
    Firefly,
    StationaryBoulder,
    FallingBoulder,
    StationaryDiamond,
    FallingDiamond,
    Explosion,
    RockfordAppearance,
    Butterfly,
    Rockford,
    Amoeba,
};

inline constexpr CellCode kEmpty = 0x00;
inline constexpr CellCode kDirt = 0x01;
inline constexpr CellCode kHiddenOutbox = 0x04;
inline constexpr CellCode kOpenOutbox = 0x05;
inline constexpr CellCode kSteelWall = 0x07;
inline constexpr CellCode kFireflyLeft = 0x08;
inline constexpr CellCode kFireflyUp = 0x09;
inline constexpr CellCode kFireflyRight = 0x0A;
inline constexpr CellCode kFireflyDown = 0x0B;
inline constexpr CellCode kBrickWall = 0x02;
inline constexpr CellCode kMagicWall = 0x03;
inline constexpr CellCode kStationaryBoulder = 0x10;
inline constexpr CellCode kScannedStationaryBoulder = 0x11;
inline constexpr CellCode kFallingBoulder = 0x12;
inline constexpr CellCode kScannedFallingBoulder = 0x13;
inline constexpr CellCode kStationaryDiamond = 0x14;
inline constexpr CellCode kScannedStationaryDiamond = 0x15;
inline constexpr CellCode kFallingDiamond = 0x16;
inline constexpr CellCode kScannedFallingDiamond = 0x17;
inline constexpr CellCode kExplosionToSpaceStart = 0x1B;
inline constexpr CellCode kExplosionToSpaceStage1 = 0x1C;
inline constexpr CellCode kExplosionToDiamondStart = 0x20;
inline constexpr CellCode kExplosionToDiamondStage1 = 0x21;
inline constexpr CellCode kInbox = 0x25;
inline constexpr CellCode kPreRockfordStage1 = 0x26;
inline constexpr CellCode kPreRockfordStage2 = 0x27;
inline constexpr CellCode kPreRockfordStage3 = 0x28;
inline constexpr CellCode kRockford = 0x38;
inline constexpr CellCode kScannedRockford = 0x39;
inline constexpr CellCode kAmoeba = 0x3A;
inline constexpr CellCode kScannedAmoeba = 0x3B;

// Direct translation of the active entries in ObjHandlerVectorTable at $5f68.
inline constexpr std::array<ObjectHandler, 0x40> kObjectHandlerTable = {
    // $00-$03
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    // $04-$07
    ObjectHandler::HiddenOutbox,
    ObjectHandler::InAndOutBoxes,
    ObjectHandler::None,
    ObjectHandler::None,
    // $08-$0b
    ObjectHandler::Firefly,
    ObjectHandler::Firefly,
    ObjectHandler::Firefly,
    ObjectHandler::Firefly,
    // $0c-$0f
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    // $10-$13
    ObjectHandler::StationaryBoulder,
    ObjectHandler::None,
    ObjectHandler::FallingBoulder,
    ObjectHandler::None,
    // $14-$17
    ObjectHandler::StationaryDiamond,
    ObjectHandler::None,
    ObjectHandler::FallingDiamond,
    ObjectHandler::None,
    // $18-$1b
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::Explosion,
    // $1c-$1f
    ObjectHandler::Explosion,
    ObjectHandler::Explosion,
    ObjectHandler::Explosion,
    ObjectHandler::Explosion,
    // $20-$23
    ObjectHandler::Explosion,
    ObjectHandler::Explosion,
    ObjectHandler::Explosion,
    ObjectHandler::Explosion,
    // $24-$27
    ObjectHandler::Explosion,
    ObjectHandler::InAndOutBoxes,
    ObjectHandler::RockfordAppearance,
    ObjectHandler::RockfordAppearance,
    // $28-$2b
    ObjectHandler::RockfordAppearance,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    // $2c-$2f
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    // $30-$33
    ObjectHandler::Butterfly,
    ObjectHandler::Butterfly,
    ObjectHandler::Butterfly,
    ObjectHandler::Butterfly,
    // $34-$37
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    // $38-$3b
    ObjectHandler::Rockford,
    ObjectHandler::None,
    ObjectHandler::Amoeba,
    ObjectHandler::None,
    // $3c-$3f
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
    ObjectHandler::None,
};

[[nodiscard]] constexpr ObjectHandler objectHandlerForCode(const CellCode code) noexcept
{
    return code < kObjectHandlerTable.size() ? kObjectHandlerTable[code] : ObjectHandler::None;
}

// Direct translation of BaseCharNoForObjectTable at $5f28.
inline constexpr std::array<CellCode, 0x40> kBaseCharacterForObjectTable = {
    0x60, 0x46, 0x4e, 0x22, 0x2e, 0x62, 0x2e, 0x4a, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
    0x44, 0x44, 0x44, 0x44, 0x48, 0x48, 0x48, 0x48, 0x00, 0x00, 0x00, 0x66, 0x68, 0x6a, 0x68, 0x66,
    0x24, 0x26, 0x28, 0x2a, 0x2c, 0x62, 0x66, 0x68, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x4c, 0x4c, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
};

[[nodiscard]] constexpr CellCode baseCharacterForObject(const CellCode code) noexcept
{
    return code < kBaseCharacterForObjectTable.size() ? kBaseCharacterForObjectTable[code] : kEmpty;
}

// Direct translation of ObjCodeFromScannedThisTickCodeTable at $5ee8.
inline constexpr std::array<CellCode, 0x40> kScannedToBaseCode = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x09, 0x0a, 0x0b,
    0x00, 0x10, 0x00, 0x12, 0x00, 0x14, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x30, 0x31, 0x32, 0x33, 0x00, 0x38, 0x00, 0x3a, 0x00, 0x00, 0x00, 0x00,
};

[[nodiscard]] constexpr CellCode normalizeScannedCode(const CellCode code) noexcept
{
    return code < kScannedToBaseCode.size() ? kScannedToBaseCode[code] : kEmpty;
}

} // namespace boulderdash::engine::objectcodes
