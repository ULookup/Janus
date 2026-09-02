#pragma once

#include "Core/Event/Event.h"

#include <bitset>

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

private:
    static constexpr usize KeyCount = static_cast<usize>(KeyCode::Count);

    std::bitset<KeyCount> m_Down;
    std::bitset<KeyCount> m_Pressed;
    std::bitset<KeyCount> m_Released;
};

} // namespace Janus
