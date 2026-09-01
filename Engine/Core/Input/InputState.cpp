#include "Core/Input/InputState.h"

namespace Janus
{

void InputState::BeginFrame() noexcept
{
    m_Pressed.reset();
    m_Released.reset();
}

void InputState::Apply(const Event& event) noexcept
{
    if (const auto* pressed = std::get_if<KeyPressedEvent>(&event))
    {
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
        const auto index = static_cast<usize>(released->key);
        m_Down.reset(index);
        m_Released.set(index);
    }
}

bool InputState::IsKeyDown(const KeyCode key) const noexcept
{
    return m_Down.test(static_cast<usize>(key));
}

bool InputState::WasKeyPressed(const KeyCode key) const noexcept
{
    return m_Pressed.test(static_cast<usize>(key));
}

bool InputState::WasKeyReleased(const KeyCode key) const noexcept
{
    return m_Released.test(static_cast<usize>(key));
}

} // namespace Janus
