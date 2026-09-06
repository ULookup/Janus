#include "Core/Input/InputState.h"
#include <cmath>

namespace Janus
{

namespace
{

[[nodiscard]] constexpr bool IsValidKeyCode(const KeyCode key) noexcept
{
    return static_cast<usize>(key) < static_cast<usize>(KeyCode::Count);
}

} // namespace

void InputState::BeginFrame() noexcept
{
    m_FrameStart = m_Down;
    m_ButtonsPressed.reset();
    m_ButtonsReleased.reset();
    m_Pressed.reset();
    m_Released.reset();
}

void InputState::Apply(const Event& event) noexcept
{
    if (std::holds_alternative<WindowFocusLostEvent>(event))
    {
        m_Released |= m_Down;
        m_Down.reset();
        m_Pressed.reset();
        m_ButtonsReleased |= m_ButtonsDown;
        m_ButtonsDown.reset();
        m_ButtonsPressed.reset();
        m_Pointer.reset();
        return;
    }
    if (const auto* moved = std::get_if<PointerMovedEvent>(&event))
    {
        if (std::isfinite(moved->position.x) && std::isfinite(moved->position.y))
            m_Pointer = moved->position;
        return;
    }
    if (const auto* pressed = std::get_if<PointerButtonPressedEvent>(&event))
    {
        const auto i = static_cast<usize>(pressed->button);
        if (i < ButtonCount)
        {
            if (!m_ButtonsDown[i])
                m_ButtonsPressed.set(i);
            m_ButtonsDown.set(i);
        }
        return;
    }
    if (const auto* released = std::get_if<PointerButtonReleasedEvent>(&event))
    {
        const auto i = static_cast<usize>(released->button);
        if (i < ButtonCount)
        {
            m_ButtonsReleased.set(i);
            m_ButtonsDown.reset(i);
        }
        return;
    }
    if (const auto* pressed = std::get_if<KeyPressedEvent>(&event))
    {
        if (!IsValidKeyCode(pressed->key))
        {
            return;
        }

        const auto index = static_cast<usize>(pressed->key);
        if (!pressed->repeat && !m_Down.test(index))
        {
            m_Pressed.set(index);
        }
        m_Down.set(index);
        return;
    }

    if (const auto* released = std::get_if<KeyReleasedEvent>(&event))
    {
        if (!IsValidKeyCode(released->key))
        {
            return;
        }

        const auto index = static_cast<usize>(released->key);
        m_Down.reset(index);
        m_Released.set(index);
    }
}

bool InputState::IsKeyDown(const KeyCode key) const noexcept
{
    if (!IsValidKeyCode(key))
    {
        return false;
    }

    return m_Down.test(static_cast<usize>(key));
}

bool InputState::WasKeyPressed(const KeyCode key) const noexcept
{
    if (!IsValidKeyCode(key))
    {
        return false;
    }

    return m_Pressed.test(static_cast<usize>(key));
}

bool InputState::WasKeyReleased(const KeyCode key) const noexcept
{
    if (!IsValidKeyCode(key))
    {
        return false;
    }

    return m_Released.test(static_cast<usize>(key));
}

bool InputState::WasKeyDownAtFrameStart(KeyCode key) const noexcept
{
    return IsValidKeyCode(key) && m_FrameStart[static_cast<usize>(key)];
}
bool InputState::IsPointerButtonDown(PointerButton button) const noexcept
{
    const auto i = static_cast<usize>(button);
    return i < ButtonCount && m_ButtonsDown[i];
}
bool InputState::WasPointerButtonPressed(PointerButton button) const noexcept
{
    const auto i = static_cast<usize>(button);
    return i < ButtonCount && m_ButtonsPressed[i];
}
bool InputState::WasPointerButtonReleased(PointerButton button) const noexcept
{
    const auto i = static_cast<usize>(button);
    return i < ButtonCount && m_ButtonsReleased[i];
}
InputState InputState::ForViewport(Vector2 origin, Vector2 displaySize, Vector2 logicalSize,
                                   bool enabled) const noexcept
{
    if (!enabled || !m_Pointer || !std::isfinite(origin.x) || !std::isfinite(origin.y) ||
        !std::isfinite(displaySize.x) || !std::isfinite(displaySize.y) ||
        !std::isfinite(logicalSize.x) || !std::isfinite(logicalSize.y) || displaySize.x <= 0 ||
        displaySize.y <= 0 || logicalSize.x <= 0 || logicalSize.y <= 0)
        return {};
    const Vector2 local{m_Pointer->x - origin.x, m_Pointer->y - origin.y};
    if (local.x < 0 || local.y < 0 || local.x >= displaySize.x || local.y >= displaySize.y)
        return {};
    auto result = *this;
    result.m_Pointer =
        Vector2{local.x / displaySize.x * logicalSize.x, local.y / displaySize.y * logicalSize.y};
    return result;
}
} // namespace Janus
