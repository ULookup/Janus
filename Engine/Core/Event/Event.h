#pragma once

#include "Core/Types.h"

#include <variant>

namespace Janus
{

enum class KeyCode : u16
{
    Escape,
    Space,
    Enter,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    W,
    A,
    S,
    D,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Count
};

struct WindowCloseEvent final
{
};

struct WindowResizeEvent final
{
    u32 width = 0;
    u32 height = 0;
};

struct KeyPressedEvent final
{
    KeyCode key = KeyCode::Escape;
    bool repeat = false;
};

struct KeyReleasedEvent final
{
    KeyCode key = KeyCode::Escape;
};

using Event = std::variant<
    WindowCloseEvent,
    WindowResizeEvent,
    KeyPressedEvent,
    KeyReleasedEvent>;

} // namespace Janus
