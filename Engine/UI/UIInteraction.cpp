#include "UI/UIInteraction.h"
#include "Scene/Scene.h"
#include <algorithm>
namespace Janus
{
UIInputResult UIInteraction::Process(const Scene& scene, const UILayoutResult& layout,
                                     const InputState& input)
{
    UIInputResult result{input, {}};
    std::vector<UUID> buttons;
    for (const auto& item : layout.items)
        if (item.kind == UIDrawKind::Button)
        {
            const auto* b = scene.GetComponent<ButtonComponent>(scene.FindEntity(item.entity));
            if (b && b->enabled && b->interactable)
                buttons.push_back(item.entity);
        }
    auto valid = [&](UUID id)
    { return id.IsValid() && std::find(buttons.begin(), buttons.end(), id) != buttons.end(); };
    auto hit = [&](std::optional<Vector2> point) -> UUID
    {
        if (!point)
            return {};
        auto entity = scene.FindEntity(layout.HitTest(*point));
        while (entity.IsValid())
        {
            const auto* button = scene.GetComponent<ButtonComponent>(entity);
            if (button)
            {
                auto id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
                // Disabled buttons block siblings underneath; labels resolve to their owning
                // button.
                for (const auto& item : layout.items)
                    if (item.kind == UIDrawKind::Button && item.entity == id &&
                        item.visible.Contains(*point))
                        return id;
                return {};
            }
            const auto* hierarchy = scene.GetComponent<HierarchyComponent>(entity);
            entity = hierarchy ? hierarchy->parent : ECS::Entity{};
        }
        return {};
    };
    if (!valid(m_State.focused))
        m_State.focused = {};
    if (!valid(m_PointerCapture))
        m_PointerCapture = {};
    if (!valid(m_KeyCapture))
        m_KeyCapture = {};
    auto consumedKeys = m_KeysOwned;
    bool consumedPointer = m_PointerOwned;
    auto point = input.GetFrameStartPointer();
    auto physical = m_Physical;
    physical.BeginFrame();
    const bool skipJournal = m_Primed && input.GetRevision() == m_PrimedRevision;
    m_Primed = false;
    if (input.EventsOverflowed())
    {
        Cancel();
        // An incomplete journal cannot prove a valid click. Quarantine UI controls until release.
        if (!buttons.empty())
        {
            consumedPointer = m_PointerOwned = true;
            for (auto key : {KeyCode::Enter, KeyCode::Space, KeyCode::ArrowUp, KeyCode::ArrowDown})
            {
                consumedKeys.set(static_cast<usize>(key));
                m_KeysOwned.set(static_cast<usize>(key));
            }
        }
    }
    else if (!skipJournal)
        for (const auto& event : input.GetEvents())
        {
            if (std::holds_alternative<WindowFocusLostEvent>(event))
            {
                Cancel();
                point.reset();
            }
            else if (const auto* move = std::get_if<PointerMovedEvent>(&event))
            {
                point = move->position;
                if (!layout.screen.Contains(*point))
                    Cancel();
                if (m_PointerCapture.IsValid() && hit(point) != m_PointerCapture)
                    m_PointerCapture = {};
            }
            else if (const auto* press = std::get_if<PointerButtonPressedEvent>(&event))
            {
                if (press->button == PointerButton::Left &&
                    !physical.IsPointerButtonDown(press->button))
                {
                    m_PointerCapture = {};
                    m_PointerOwned = false;
                    const auto target = hit(point);
                    if (target.IsValid())
                    {
                        consumedPointer = m_PointerOwned = true;
                        if (valid(target))
                        {
                            m_PointerCapture = target;
                            m_State.focused = target;
                        }
                    }
                    else
                        m_State.focused = {};
                }
            }
            else if (const auto* release = std::get_if<PointerButtonReleasedEvent>(&event))
            {
                if (release->button == PointerButton::Left)
                {
                    if (m_PointerCapture.IsValid() && hit(point) == m_PointerCapture)
                        result.clicks.push_back(m_PointerCapture);
                    m_PointerCapture = {};
                    m_PointerOwned = false;
                }
            }
            else if (const auto* keyPress = std::get_if<KeyPressedEvent>(&event))
            {
                if (!keyPress->repeat && !physical.IsKeyDown(keyPress->key) && !buttons.empty() &&
                    (!point || layout.screen.Contains(*point)))
                {
                    const bool navigate =
                        keyPress->key == KeyCode::ArrowDown || keyPress->key == KeyCode::ArrowUp;
                    const bool confirm =
                        keyPress->key == KeyCode::Enter || keyPress->key == KeyCode::Space;
                    if (navigate || (confirm && valid(m_State.focused)))
                    {
                        consumedKeys.set(static_cast<usize>(keyPress->key));
                        m_KeysOwned.set(static_cast<usize>(keyPress->key));
                        if (navigate)
                        {
                            auto it = std::find(buttons.begin(), buttons.end(), m_State.focused);
                            const auto index =
                                it == buttons.end()
                                    ? (keyPress->key == KeyCode::ArrowDown ? 0 : buttons.size() - 1)
                                    : (static_cast<usize>(it - buttons.begin()) + buttons.size() +
                                       (keyPress->key == KeyCode::ArrowDown ? 1 : -1)) %
                                          buttons.size();
                            m_State.focused = buttons[index];
                            m_KeyCapture = {};
                        }
                        else if (!m_KeyCapture.IsValid())
                        {
                            m_KeyCapture = m_State.focused;
                            m_ConfirmKey = keyPress->key;
                        }
                    }
                }
            }
            else if (const auto* keyRelease = std::get_if<KeyReleasedEvent>(&event))
            {
                const auto i = static_cast<usize>(keyRelease->key);
                if (i < m_KeysOwned.size())
                    m_KeysOwned.reset(i);
                if (keyRelease->key == m_ConfirmKey)
                {
                    if (valid(m_KeyCapture) && m_KeyCapture == m_State.focused)
                        result.clicks.push_back(m_KeyCapture);
                    m_KeyCapture = {};
                }
            }
            physical.Apply(event);
        }
    if (!input.IsPointerButtonDown(PointerButton::Left))
        m_PointerCapture = {};

    if (!input.IsKeyDown(m_ConfirmKey))
        m_KeyCapture = {};
    if (consumedPointer)
        result.gameplay.SuppressPointerButton(PointerButton::Left,
                                              m_Gameplay.IsPointerButtonDown(PointerButton::Left));
    for (usize i = 0; i < consumedKeys.size(); ++i)
        if (consumedKeys[i])
            result.gameplay.SuppressKey(static_cast<KeyCode>(i),
                                        m_Gameplay.IsKeyDown(static_cast<KeyCode>(i)));
    m_State.hovered = hit(input.GetPointerPosition());
    m_State.pressed = m_PointerCapture.IsValid() ? m_PointerCapture : m_KeyCapture;
    m_Physical = input;
    m_Gameplay = result.gameplay;
    return result;
}
} // namespace Janus
