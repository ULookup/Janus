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
    ++m_Revision;
    m_EventCount = 0;
    m_Overflow = false;
    m_StartPointer = m_Pointer;
    m_FrameStart = m_Down;
    m_ButtonsPressed.reset();
    m_ButtonsReleased.reset();
    m_Pressed.reset();
    m_Released.reset();
}

void InputState::Apply(const Event& event) noexcept
{
    ++m_Revision;
    if (m_EventCount < m_Events.size())
        m_Events[m_EventCount++] = event;
    else
        m_Overflow = true;
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
    auto result = MapToViewport(origin, displaySize, logicalSize);
    auto point = result.GetFrameStartPointer();
    std::bitset<KeyCount> outsideKeys;
    std::bitset<ButtonCount> outsideButtons;
    for (const auto& event : result.GetEvents())
    {
        if (const auto* move = std::get_if<PointerMovedEvent>(&event))
            point = move->position;
        const bool inside = point && point->x >= 0 && point->y >= 0 && point->x < logicalSize.x &&
                            point->y < logicalSize.y;
        if (!inside)
        {
            if (const auto* key = std::get_if<KeyPressedEvent>(&event);
                key && IsValidKeyCode(key->key))
                outsideKeys.set(static_cast<usize>(key->key));
            if (const auto* button = std::get_if<PointerButtonPressedEvent>(&event);
                button && static_cast<usize>(button->button) < ButtonCount)
                outsideButtons.set(static_cast<usize>(button->button));
        }
    }
    for (usize i = 0; i < KeyCount; ++i)
        if (outsideKeys[i])
            result.SuppressKey(static_cast<KeyCode>(i));
    for (usize i = 0; i < ButtonCount; ++i)
        if (outsideButtons[i])
            result.SuppressPointerButton(static_cast<PointerButton>(i));
    return result;
}
InputState InputState::MapToViewport(Vector2 origin, Vector2 displaySize,
                                     Vector2 logicalSize) const noexcept
{
    auto result = *this;
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(displaySize.x) ||
        !std::isfinite(displaySize.y) || !std::isfinite(logicalSize.x) ||
        !std::isfinite(logicalSize.y) || displaySize.x <= 0 || displaySize.y <= 0 ||
        logicalSize.x <= 0 || logicalSize.y <= 0)
        return result;
    auto map = [&](Vector2 p)
    {
        return Vector2{(p.x - origin.x) / displaySize.x * logicalSize.x,
                       (p.y - origin.y) / displaySize.y * logicalSize.y};
    };
    if (result.m_Pointer)
        result.m_Pointer = map(*result.m_Pointer);
    if (result.m_StartPointer)
        result.m_StartPointer = map(*result.m_StartPointer);
    for (usize i = 0; i < result.m_EventCount; ++i)
        if (auto* moved = std::get_if<PointerMovedEvent>(&result.m_Events[i]))
            moved->position = map(moved->position);
    return result;
}
void InputState::SuppressKey(KeyCode key, bool releasePrevious) noexcept
{
    const auto i = static_cast<usize>(key);
    if (i >= KeyCount)
        return;
    m_Down.reset(i);
    m_Pressed.reset(i);
    m_FrameStart.set(i, releasePrevious);
    m_Released.set(i, releasePrevious);
    usize count = 0;
    for (usize j = 0; j < m_EventCount; ++j)
    {
        const auto* press = std::get_if<KeyPressedEvent>(&m_Events[j]);
        const auto* release = std::get_if<KeyReleasedEvent>(&m_Events[j]);
        if ((press && press->key == key) || (release && release->key == key))
            continue;
        m_Events[count++] = m_Events[j];
    }
    m_EventCount = count;
    if (releasePrevious && m_EventCount < m_Events.size())
        m_Events[m_EventCount++] = KeyReleasedEvent{key};
}
void InputState::SuppressPointerButton(PointerButton button, bool releasePrevious) noexcept
{
    const auto i = static_cast<usize>(button);
    if (i >= ButtonCount)
        return;
    m_ButtonsDown.reset(i);
    m_ButtonsPressed.reset(i);
    m_ButtonsReleased.set(i, releasePrevious);
    usize count = 0;
    for (usize j = 0; j < m_EventCount; ++j)
    {
        const auto* press = std::get_if<PointerButtonPressedEvent>(&m_Events[j]);
        const auto* release = std::get_if<PointerButtonReleasedEvent>(&m_Events[j]);
        if ((press && press->button == button) || (release && release->button == button))
            continue;
        m_Events[count++] = m_Events[j];
    }
    m_EventCount = count;
    if (releasePrevious && m_EventCount < m_Events.size())
        m_Events[m_EventCount++] = PointerButtonReleasedEvent{button};
}
} // namespace Janus
