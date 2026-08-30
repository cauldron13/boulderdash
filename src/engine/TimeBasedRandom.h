#pragma once

#include <cstdint>

namespace boulderdash::engine
{

struct CiaTimerSample final
{
    std::uint8_t cia1TimerALow = 0;
    std::uint8_t cia1TimerAHigh = 0;
    std::uint8_t cia2TimerALow = 0;
    std::uint8_t cia2TimerAHigh = 0;
    std::uint8_t cia2TimerBLow = 0;
    std::uint8_t cia2TimerBHigh = 0;
};

struct TimeBasedRandomResult final
{
    std::uint8_t value = 0;
    bool carry = false;
};

[[nodiscard]] std::uint8_t timeBasedRandomNumber(const CiaTimerSample &sample, bool inputCarry) noexcept;
[[nodiscard]] TimeBasedRandomResult timeBasedRandomNumberWithCarry(const CiaTimerSample &sample,
                                                                   bool inputCarry) noexcept;

struct TimeBasedRandomState final
{
    CiaTimerSample sample;
    std::uint32_t callsConsumed = 0;

    [[nodiscard]] std::uint8_t next(bool inputCarry) noexcept;
    [[nodiscard]] TimeBasedRandomResult nextWithCarry(bool inputCarry) noexcept;
};

} // namespace boulderdash::engine
