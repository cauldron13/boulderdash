#pragma once

#include "engine/EngineTypes.h"

namespace boulderdash::app
{

class KeyboardInput final
{
  public:
    [[nodiscard]] bool pressKey(int key) noexcept;
    [[nodiscard]] bool releaseKey(int key) noexcept;
    void clear() noexcept;
    [[nodiscard]] engine::GameCommand command() const noexcept;

  private:
    bool upArrowPressed_ = false;
    bool zPressed_ = false;
    bool downArrowPressed_ = false;
    bool sPressed_ = false;
    bool leftArrowPressed_ = false;
    bool qPressed_ = false;
    bool rightArrowPressed_ = false;
    bool dPressed_ = false;
    bool firePressed_ = false;
};

} // namespace boulderdash::app
