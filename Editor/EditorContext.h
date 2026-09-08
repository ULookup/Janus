#pragma once

#include "Core/UUID/UUID.h"
#include "ECS/Entity.h"

#include <optional>

namespace Janus
{

class Scene;
class Renderer2D;

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

// Value-only payload remains valid across asset registry reallocations. A new session
// has a fresh identity even when reopening the same project path.
struct EditorAssetPayload
{
    UUID project;
    UUID asset;
};
inline constexpr const char* EditorAssetPayloadType = "JANUS_ASSET";

struct EditorContext
{
    ProjectSession* project = nullptr;
    Renderer2D* renderer = nullptr;
    EditorSelection selection;
    std::optional<UUID> locateAsset;
};

} // namespace Editor
} // namespace Janus
