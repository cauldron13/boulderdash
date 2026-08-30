#include "engine/TimeBasedRandom.h"

namespace boulderdash::engine
{
namespace
{

void decrementTimer(std::uint8_t &low, std::uint8_t &high) noexcept
{
    const std::uint16_t timer = static_cast<std::uint16_t>(low) | (static_cast<std::uint16_t>(high) << 8U);
    const std::uint16_t decremented = static_cast<std::uint16_t>(timer - 33U);
    low = static_cast<std::uint8_t>(decremented);
    high = static_cast<std::uint8_t>(decremented >> 8U);
}

} // namespace

std::uint8_t timeBasedRandomNumber(const CiaTimerSample &sample, const bool inputCarry) noexcept
{
    return timeBasedRandomNumberWithCarry(sample, inputCarry).value;
}

TimeBasedRandomResult timeBasedRandomNumberWithCarry(const CiaTimerSample &sample, const bool inputCarry) noexcept
{
    std::uint8_t result = static_cast<std::uint8_t>(sample.cia1TimerALow ^ sample.cia1TimerAHigh);
    result = static_cast<std::uint8_t>(result ^ sample.cia2TimerALow);
    const std::uint16_t sum = static_cast<std::uint16_t>(result) + sample.cia2TimerAHigh + inputCarry;
    result = static_cast<std::uint8_t>(sum);
    result = static_cast<std::uint8_t>(result ^ sample.cia2TimerBLow);
    return {static_cast<std::uint8_t>(result ^ sample.cia2TimerBHigh), sum > 0xffU};
}

std::uint8_t TimeBasedRandomState::next(const bool inputCarry) noexcept
{
    return nextWithCarry(inputCarry).value;
}

TimeBasedRandomResult TimeBasedRandomState::nextWithCarry(const bool inputCarry) noexcept
{
    ++callsConsumed;
    const TimeBasedRandomResult result = timeBasedRandomNumberWithCarry(sample, inputCarry);
    decrementTimer(sample.cia1TimerALow, sample.cia1TimerAHigh);
    decrementTimer(sample.cia2TimerALow, sample.cia2TimerAHigh);
    decrementTimer(sample.cia2TimerBLow, sample.cia2TimerBHigh);
    return result;
}

} // namespace boulderdash::engine
