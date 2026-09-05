#include "EditorActions.h"

#include "EditorContext.h"
#include "ProjectSession.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <utility>

namespace Janus::Editor
{

EditorActions::EditorActions(
    EditorContext& context) noexcept
    : m_Context(context)
{
}

Result<Scene*> EditorActions::GetEditableScene()
{
    if (m_Context.project == nullptr)
    {
        return Result<Scene*>::Failure(
            ErrorCode::InvalidState,
            "Editor action requires an open project.");
    }

    if (m_Context.project->IsPlaying())
    {
        return Result<Scene*>::Failure(
            ErrorCode::InvalidState,
            "Authoring mutations are disabled while Play Mode is active.");
    }

    return Result<Scene*>::Success(
        &m_Context.project->GetEditorScene());
}

Result<ECS::Entity> EditorActions::ResolveEntity(
    Scene& scene,
    UUID id)
{
    if (!id.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::InvalidArgument,
            "Editor action requires a valid entity UUID.");
    }

    const ECS::Entity entity = scene.FindEntity(id);
    if (!entity.IsValid())
    {
        return Result<ECS::Entity>::Failure(
            ErrorCode::EntityNotFound,
            "Editor entity no longer exists.");
    }

    return Result<ECS::Entity>::Success(entity);
}

Result<UUID> EditorActions::CreateEntity(
    std::string name)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<UUID>::Failure(
            editable.GetError());
    }

    if (name.empty())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "Entity name cannot be empty.");
    }

    Scene& scene = *editable.Value();
    const ECS::Entity entity =
        scene.CreateEntity(std::move(name));

    const auto* identity =
        scene.GetComponent<EntityIdentityComponent>(entity);
    if (identity == nullptr)
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidState,
            "Created entity has no identity component.");
    }

    m_Context.selection.Select(identity->id);
    return Result<UUID>::Success(identity->id);
}

Result<void> EditorActions::DeleteEntity(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (!scene.DestroyEntity(entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidEntity,
            "Failed to delete Editor entity.");
    }

    m_Context.selection.Validate(scene);
    return Result<void>::Success();
}

Result<void> EditorActions::RenameEntity(
    UUID id,
    std::string name)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    if (name.empty())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Entity name cannot be empty.");
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    auto* identity =
        scene.GetComponent<EntityIdentityComponent>(
            entity.Value());
    if (identity == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity identity component is missing.");
    }

    identity->name = std::move(name);
    return Result<void>::Success();
}

Result<void> EditorActions::SetTransform(
    UUID id,
    Vector2 position,
    f32 rotationRadians,
    Vector2 scale)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    auto* transform =
        scene.GetComponent<TransformComponent>(
            entity.Value());
    if (transform == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity transform component is missing.");
    }

    transform->position = position;
    transform->rotationRadians = rotationRadians;
    transform->scale = scale;
    transform->dirty = true;

    return Result<void>::Success();
}

Result<void> EditorActions::AddSpriteRenderer(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (scene.HasComponent<SpriteRendererComponent>(
            entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity already has a SpriteRenderer component.");
    }

    scene.AddComponent<SpriteRendererComponent>(
        entity.Value(),
        SpriteRendererComponent{});

    return Result<void>::Success();
}

Result<void> EditorActions::RemoveSpriteRenderer(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (!scene.RemoveComponent<SpriteRendererComponent>(
            entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity has no SpriteRenderer component.");
    }

    return Result<void>::Success();
}

Result<void> EditorActions::SetSpriteRenderer(
    UUID id,
    Vector2 size,
    Color color,
    i32 layer,
    bool enabled)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    auto* sprite =
        scene.GetComponent<SpriteRendererComponent>(
            entity.Value());
    if (sprite == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity has no SpriteRenderer component.");
    }

    sprite->size = size;
    sprite->color = color;
    sprite->layer = layer;
    sprite->enabled = enabled;

    return Result<void>::Success();
}

Result<void> EditorActions::AddCamera(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (scene.HasComponent<CameraComponent>(entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity already has a Camera component.");
    }

    scene.AddComponent<CameraComponent>(
        entity.Value(),
        CameraComponent{});

    return Result<void>::Success();
}

Result<void> EditorActions::RemoveCamera(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (!scene.RemoveComponent<CameraComponent>(
            entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity has no Camera component.");
    }

    return Result<void>::Success();
}

Result<void> EditorActions::SetCamera(
    UUID id,
    f32 zoom,
    bool primary)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    if (zoom <= 0.0f)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Camera zoom must be positive.");
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    auto* target =
        scene.GetComponent<CameraComponent>(
            entity.Value());
    if (target == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity has no Camera component.");
    }

    if (primary)
    {
        scene.View<CameraComponent>()
            .ForEach(
                [](ECS::Entity, CameraComponent& camera)
                {
                    camera.primary = false;
                });
    }

    target->zoom = zoom;
    target->primary = primary;

    return Result<void>::Success();
}

Result<void> EditorActions::AddLuaScript(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (scene.HasComponent<LuaScriptComponent>(
            entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity already has a LuaScript component.");
    }

    // A disabled nil script is a valid serializable authoring state.
    scene.AddComponent<LuaScriptComponent>(
        entity.Value(),
        LuaScriptComponent{AssetHandle{}, false});

    return Result<void>::Success();
}

Result<void> EditorActions::RemoveLuaScript(UUID id)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    if (!scene.RemoveComponent<LuaScriptComponent>(
            entity.Value()))
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity has no LuaScript component.");
    }

    return Result<void>::Success();
}

Result<void> EditorActions::SetLuaScriptEnabled(
    UUID id,
    bool enabled)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    auto entity = ResolveEntity(scene, id);
    if (!entity)
    {
        return Result<void>::Failure(
            entity.GetError());
    }

    auto* script =
        scene.GetComponent<LuaScriptComponent>(
            entity.Value());
    if (script == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Entity has no LuaScript component.");
    }

    if (enabled && !script->script.IsValid())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Cannot enable LuaScript without a Script AssetHandle.");
    }

    script->enabled = enabled;
    return Result<void>::Success();
}

} // namespace Janus::Editor
