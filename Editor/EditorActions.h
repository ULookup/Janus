#pragma once

#include "Core/Error/Result.h"
#include "Core/Math/Vector2.h"
#include "Core/UUID/UUID.h"
#include "ECS/Entity.h"
#include "Renderer/RendererTypes.h"

#include <string>

namespace Janus
{

class Scene;

namespace Editor
{

struct EditorContext;

class EditorActions final
{
public:
    explicit EditorActions(EditorContext& context) noexcept;

    [[nodiscard]] Result<UUID> CreateEntity(std::string name);
    [[nodiscard]] Result<void> DeleteEntity(UUID id);
    [[nodiscard]] Result<void> RenameEntity(
        UUID id,
        std::string name);

    [[nodiscard]] Result<void> SetTransform(
        UUID id,
        Vector2 position,
        f32 rotationRadians,
        Vector2 scale);

    [[nodiscard]] Result<void> AddSpriteRenderer(UUID id);
    [[nodiscard]] Result<void> RemoveSpriteRenderer(UUID id);
    [[nodiscard]] Result<void> SetSpriteRenderer(
        UUID id,
        Vector2 size,
        Color color,
        i32 layer,
        bool enabled);

    [[nodiscard]] Result<void> AddCamera(UUID id);
    [[nodiscard]] Result<void> RemoveCamera(UUID id);
    [[nodiscard]] Result<void> SetCamera(
        UUID id,
        f32 zoom,
        bool primary);

    [[nodiscard]] Result<void> AddLuaScript(UUID id);
    [[nodiscard]] Result<void> RemoveLuaScript(UUID id);
    [[nodiscard]] Result<void> SetLuaScriptEnabled(
        UUID id,
        bool enabled);

private:
    [[nodiscard]] Result<Scene*> GetEditableScene();
    [[nodiscard]] Result<ECS::Entity> ResolveEntity(
        Scene& scene,
        UUID id);

    EditorContext& m_Context;
};

} // namespace Editor
} // namespace Janus
