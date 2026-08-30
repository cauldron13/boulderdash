#pragma once

#include <cstdint>

namespace boulderdash::engine
{

// VICE PAL calibration of SubSecondTick ($7111-$712c) on the reference D64.
inline constexpr std::uint64_t kC64PalCpuCyclesPerSecond = 985248;
inline constexpr std::uint64_t kC64PalCyclesPerCalibrationWindow = 11597040;
inline constexpr std::uint32_t kC64PalSubSecondTicksPerCalibrationWindow = 600;

// The C64 advances a game second after 0x3c SubSecondTick calls.
inline constexpr std::uint32_t kC64SubSecondTicksPerGameSecond = 0x3c;

} // namespace boulderdash::engine
