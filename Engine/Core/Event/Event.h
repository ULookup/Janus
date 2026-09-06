#pragma once

#include "Core/Math/Vector2.h"
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

enum class PointerButton : u8
{
    Left,
    Right,
    Middle,
    Count
};
struct PointerMovedEvent final
{
    Vector2 position;
};
struct PointerButtonPressedEvent final
{
    PointerButton button;
};
struct PointerButtonReleasedEvent final
{
    PointerButton button;
};
struct WindowFocusLostEvent final
{
};

using Event = std::variant<WindowCloseEvent, WindowResizeEvent, KeyPressedEvent, KeyReleasedEvent,
                           PointerMovedEvent, PointerButtonPressedEvent, PointerButtonReleasedEvent,
                           WindowFocusLostEvent>;

} // namespace Janus
