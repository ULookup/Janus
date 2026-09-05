#pragma once

#include "Core/UUID/UUID.h"
#include "ECS/Entity.h"

#include <optional>

namespace Janus
{

class Scene;

namespace Editor
{

class ProjectSession;

class EditorSelection final
{
public:
    void Select(UUID id) noexcept;
    void Clear() noexcept;

    [[nodiscard]] bool HasSelection() const noexcept;
    [[nodiscard]] const std::optional<UUID>& GetSelectedUUID() const noexcept;
    [[nodiscard]] ECS::Entity Resolve(
        const Scene& scene) const noexcept;

    bool Validate(const Scene& scene) noexcept;

private:
    std::optional<UUID> m_Selected;
};

struct EditorContext
{
    ProjectSession* project = nullptr;
    EditorSelection selection;
};

} // namespace Editor
} // namespace Janus
