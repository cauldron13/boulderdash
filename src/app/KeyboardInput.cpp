#include "app/KeyboardInput.h"

#include <Qt>

namespace boulderdash::app
{
bool KeyboardInput::pressKey(const int key) noexcept
{
    switch (key)
    {
    case Qt::Key_Up:
        upArrowPressed_ = true;
        return true;
    case Qt::Key_Z:
        zPressed_ = true;
        return true;
    case Qt::Key_Down:
        downArrowPressed_ = true;
        return true;
    case Qt::Key_S:
        sPressed_ = true;
        return true;
    case Qt::Key_Left:
        leftArrowPressed_ = true;
        return true;
    case Qt::Key_Q:
        qPressed_ = true;
        return true;
    case Qt::Key_Right:
        rightArrowPressed_ = true;
        return true;
    case Qt::Key_D:
        dPressed_ = true;
        return true;
    case Qt::Key_F:
        firePressed_ = true;
        return true;
    default:
        return false;
    }
}

bool KeyboardInput::releaseKey(const int key) noexcept
{
    switch (key)
    {
    case Qt::Key_Up:
        upArrowPressed_ = false;
        return true;
    case Qt::Key_Z:
        zPressed_ = false;
        return true;
    case Qt::Key_Down:
        downArrowPressed_ = false;
        return true;
    case Qt::Key_S:
        sPressed_ = false;
        return true;
    case Qt::Key_Left:
        leftArrowPressed_ = false;
        return true;
    case Qt::Key_Q:
        qPressed_ = false;
        return true;
    case Qt::Key_Right:
        rightArrowPressed_ = false;
        return true;
    case Qt::Key_D:
        dPressed_ = false;
        return true;
    case Qt::Key_F:
        firePressed_ = false;
        return true;
    default:
        return false;
    }
}

void KeyboardInput::clear() noexcept
{
    upArrowPressed_ = false;
    zPressed_ = false;
    downArrowPressed_ = false;
    sPressed_ = false;
    leftArrowPressed_ = false;
    qPressed_ = false;
    rightArrowPressed_ = false;
    dPressed_ = false;
    firePressed_ = false;
}

engine::GameCommand KeyboardInput::command() const noexcept
{
    const bool up = upArrowPressed_ || zPressed_;
    const bool down = downArrowPressed_ || sPressed_;
    const bool left = leftArrowPressed_ || qPressed_;
    const bool right = rightArrowPressed_ || dPressed_;

    engine::Direction direction = engine::Direction::Neutral;
    // HandleJoystickForRockford ($7236) accepts vertical movement only for
    // an exclusive vertical input. Any combination containing right wins over
    // left; otherwise a combination containing left moves left.
    if (down && !up && !left && !right)
    {
        direction = engine::Direction::South;
    }
    else if (up && !down && !left && !right)
    {
        direction = engine::Direction::North;
    }
    else if (right)
    {
        direction = engine::Direction::East;
    }
    else if (left)
    {
        direction = engine::Direction::West;
    }

    return {direction, firePressed_};
}

} // namespace boulderdash::app
