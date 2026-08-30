#pragma once

#include <cstdint>

namespace boulderdash::engine
{

struct PseudoRandomState final
{
    std::uint8_t seed1 = 0;
    std::uint8_t seed2 = 0;

    [[nodiscard]] std::uint8_t next();
};

} // namespace boulderdash::engine
