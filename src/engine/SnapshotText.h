#pragma once

#include "engine/GameState.h"

#include <string>

namespace boulderdash::engine
{

[[nodiscard]] std::string serializeSnapshotV1(const GameSnapshot &snapshot);
[[nodiscard]] std::string serializeSnapshot(const GameSnapshot &snapshot);
[[nodiscard]] std::string snapshotAsciiDiagnostic(const GameSnapshot &snapshot);

} // namespace boulderdash::engine
