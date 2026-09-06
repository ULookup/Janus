#pragma once

#include "Core/Event/Event.h"

#include <bitset>
#include <optional>

namespace Janus
{

class InputState final
{
public:
    void BeginFrame() noexcept;
    void Apply(const Event& event) noexcept;

    [[nodiscard]] bool IsKeyDown(KeyCode key) const noexcept;
    [[nodiscard]] bool WasKeyPressed(KeyCode key) const noexcept;
    [[nodiscard]] bool WasKeyReleased(KeyCode key) const noexcept;
    [[nodiscard]] bool WasKeyDownAtFrameStart(KeyCode key) const noexcept;
    [[nodiscard]] bool IsPointerButtonDown(PointerButton button) const noexcept;
    [[nodiscard]] bool WasPointerButtonPressed(PointerButton button) const noexcept;
    [[nodiscard]] bool WasPointerButtonReleased(PointerButton button) const noexcept;
    [[nodiscard]] std::optional<Vector2> GetPointerPosition() const noexcept
    {
        return m_Pointer;
    }
    [[nodiscard]] InputState ForViewport(Vector2 origin, Vector2 displaySize, Vector2 logicalSize,
                                         bool enabled) const noexcept;

  private:
    static constexpr usize KeyCount = static_cast<usize>(KeyCode::Count);

    std::bitset<KeyCount> m_Down;
    std::bitset<KeyCount> m_Pressed;
    std::bitset<KeyCount> m_Released;
    std::bitset<KeyCount> m_FrameStart;
    static constexpr usize ButtonCount = static_cast<usize>(PointerButton::Count);
    std::bitset<ButtonCount> m_ButtonsDown, m_ButtonsPressed, m_ButtonsReleased;
    std::optional<Vector2> m_Pointer;
};

} // namespace Janus
