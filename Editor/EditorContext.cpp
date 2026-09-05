#include "EditorContext.h"

#include "Scene/Scene.h"

namespace Janus::Editor
{

void EditorSelection::Select(UUID id) noexcept
{
    if (!id.IsValid())
    {
        m_Selected.reset();
        return;
    }

    m_Selected = id;
}

void EditorSelection::Clear() noexcept
{
    m_Selected.reset();
}

bool EditorSelection::HasSelection() const noexcept
{
    return m_Selected.has_value();
}

const std::optional<UUID>&
EditorSelection::GetSelectedUUID() const noexcept
{
    return m_Selected;
}

ECS::Entity EditorSelection::Resolve(
    const Scene& scene) const noexcept
{
    if (!m_Selected.has_value())
    {
        return {};
    }

    return scene.FindEntity(*m_Selected);
}

bool EditorSelection::Validate(
    const Scene& scene) noexcept
{
    if (!m_Selected.has_value())
    {
        return false;
    }

    if (!scene.FindEntity(*m_Selected).IsValid())
    {
        m_Selected.reset();
        return false;
    }

    return true;
}

} // namespace Janus::Editor
