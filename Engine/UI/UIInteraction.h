#pragma once
#include "Core/Input/InputState.h"
#include "UI/UIComponents.h"
#include "UI/UILayout.h"
namespace Janus
{
struct UIInteractionState
{
    UUID hovered, focused, pressed;
};
struct UIInputResult
{
    InputState gameplay;
    std::vector<UUID> clicks;
};
// Runtime-owned transient state. Events contain persistent identities, never ECS pointers.
class UIInteraction final
{
  public:
    [[nodiscard]] UIInputResult Process(const Scene& scene, const UILayoutResult& layout,
                                        const InputState& input);
    void Prime(const InputState& input)
    {
        m_Physical = input;
        m_PrimedRevision = input.GetRevision();
        m_Primed = true;
    }
    void Cancel() noexcept
    {
        m_State = {};
        m_PointerCapture = {};
        m_KeyCapture = {};
    }
    [[nodiscard]] const UIInteractionState& GetState() const noexcept
    {
        return m_State;
    }

  private:
    UIInteractionState m_State;
    UUID m_PointerCapture, m_KeyCapture;
    KeyCode m_ConfirmKey = KeyCode::Enter;
    bool m_PointerOwned = false;
    bool m_Primed = false;
    u64 m_PrimedRevision = 0;
    std::bitset<static_cast<usize>(KeyCode::Count)> m_KeysOwned;
    InputState m_Physical, m_Gameplay;
};
} // namespace Janus
