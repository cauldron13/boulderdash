#include "engine/PseudoRandom.h"

#include <cstdint>

namespace boulderdash::engine
{

std::uint8_t PseudoRandomState::next()
{
    // Direct translation of PseudoRandom at $6ced-$6d15. The incoming 6502
    // carry does not affect the masked rotation results. Each consecutive ADC
    // keeps the carry from the preceding ADC, including across the seed update.
    const std::uint8_t seed1Feedback = static_cast<std::uint8_t>((seed1 & 0x01U) << 7U);
    const std::uint8_t shiftedSeed2 = static_cast<std::uint8_t>((seed2 >> 1U) & 0x7fU);
    const std::uint8_t seed2Feedback = static_cast<std::uint8_t>((seed2 & 0x01U) << 7U);

    const std::uint16_t firstSeed2Sum = static_cast<std::uint16_t>(seed2) + seed2Feedback;
    const std::uint16_t seed2Sum =
        static_cast<std::uint8_t>(firstSeed2Sum) + 0x13U + static_cast<std::uint16_t>(firstSeed2Sum > 0xffU);
    seed2 = static_cast<std::uint8_t>(seed2Sum);

    const std::uint16_t firstSeed1Sum =
        static_cast<std::uint16_t>(seed1) + seed1Feedback + static_cast<std::uint16_t>(seed2Sum > 0xffU);
    const std::uint16_t seed1Sum =
        static_cast<std::uint8_t>(firstSeed1Sum) + shiftedSeed2 + static_cast<std::uint16_t>(firstSeed1Sum > 0xffU);
    seed1 = static_cast<std::uint8_t>(seed1Sum);
    return seed1;
}

} // namespace boulderdash::engine
