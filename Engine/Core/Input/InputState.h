#pragma once

#include "Core/Event/Event.h"

#include <array>
#include <bitset>
#include <optional>
#include <span>

namespace Janus
{

class InputState final
{
public:
  [[nodiscard]] u64 GetRevision() const noexcept
  {
      return m_Revision;
  }
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
    [[nodiscard]] InputState MapToViewport(Vector2 origin, Vector2 displaySize,
                                           Vector2 logicalSize) const noexcept;
    [[nodiscard]] InputState ForViewport(Vector2 origin, Vector2 displaySize, Vector2 logicalSize,
                                         bool enabled) const noexcept;

    // Bounded transition journal preserves short taps and pointer movement order.
    [[nodiscard]] std::span<const Event> GetEvents() const noexcept
    {
        return {m_Events.data(), m_EventCount};
    }
    [[nodiscard]] bool EventsOverflowed() const noexcept
    {
        return m_Overflow;
    }
    [[nodiscard]] std::optional<Vector2> GetFrameStartPointer() const noexcept
    {
        return m_StartPointer;
    }
    void SuppressKey(KeyCode key, bool releasePrevious = false) noexcept;
    void SuppressPointerButton(PointerButton button, bool releasePrevious = false) noexcept;

  private:
    static constexpr usize KeyCount = static_cast<usize>(KeyCode::Count);

    std::bitset<KeyCount> m_Down;
    std::bitset<KeyCount> m_Pressed;
    std::bitset<KeyCount> m_Released;
    std::bitset<KeyCount> m_FrameStart;
    static constexpr usize ButtonCount = static_cast<usize>(PointerButton::Count);
    std::bitset<ButtonCount> m_ButtonsDown, m_ButtonsPressed, m_ButtonsReleased;
    std::optional<Vector2> m_Pointer, m_StartPointer;
    std::array<Event, 256> m_Events{};
    usize m_EventCount = 0;
    u64 m_Revision = 0;
    bool m_Overflow = false;
};

} // namespace Janus
