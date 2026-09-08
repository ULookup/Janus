#pragma once

#include "Asset/AssetHandle.h"
#include "Core/Error/Result.h"
#include "Core/Math/Vector2.h"
#include "Core/Reflection/ReflectionTypes.h"
#include "Core/UUID/UUID.h"
#include "Renderer/RendererTypes.h"

#include <memory>
#include <optional>
#include <string>

namespace Janus
{

class ICommand;
class Scene;

namespace Editor
{

struct EditorContext;
struct EditorAssetPayload;
class EditorTransformDrag;

class EditorActions final
{
public:
    explicit EditorActions(EditorContext& context) noexcept;
    [[nodiscard]] Result<void> CommitTransformDrag(EditorTransformDrag& drag);

    [[nodiscard]] Result<UUID> CreateEntity(std::string name);
    [[nodiscard]] Result<AssetHandle> ExportPrefab(UUID root,
                                                   std::optional<std::string> name = std::nullopt);
    [[nodiscard]] Result<UUID> DuplicateEntity(UUID source);
    [[nodiscard]] Result<void> AssignAssetPayload(UUID entity, ComponentTypeId component,
                                                  PropertyId property,
                                                  const EditorAssetPayload& payload);
    [[nodiscard]] Result<UUID> InstantiatePrefab(AssetHandle asset);
    [[nodiscard]] Result<void> ReparentEntity(UUID entity, UUID parent, usize siblingIndex = 0);
    [[nodiscard]] Result<void> DeleteEntity(UUID id);
    [[nodiscard]] Result<void> RenameEntity(
        UUID id,
        std::string name);

    [[nodiscard]] Result<void> SetProperty(
        UUID id,
        ComponentTypeId component,
        PropertyId property,
        PropertyValue value);

    [[nodiscard]] Result<void> AddComponent(
        UUID id,
        ComponentTypeId component);

    [[nodiscard]] Result<void> RemoveComponent(
        UUID id,
        ComponentTypeId component);

    [[nodiscard]] Result<void> Undo();
    [[nodiscard]] Result<void> Redo();

    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;

    // v0.6 compatibility helpers. These delegate to the generic
    // reflected command path rather than mutating ECS state directly.
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
    [[nodiscard]] Result<void> SetSpriteTexture(
        UUID id,
        AssetHandle texture);

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
    [[nodiscard]] Result<void> SetLuaScriptAsset(
        UUID id,
        AssetHandle script);

private:
    [[nodiscard]] Result<Scene*> GetEditableScene();

    [[nodiscard]] Result<void> ExecutePrepared(
        Scene& scene,
        std::unique_ptr<ICommand> command);

    void FinishSuccessfulMutation(
        Scene& scene) noexcept;

    EditorContext& m_Context;
};

} // namespace Editor
} // namespace Janus
