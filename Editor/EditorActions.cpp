#include "EditorActions.h"

#include "EditorContext.h"
#include "ProjectSession.h"

#include "Core/Command/ICommand.h"
#include "Scene/Command/EntityCommands.h"
#include "Scene/Command/SceneCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneReflection.h"

#include <memory>
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

void EditorActions::FinishSuccessfulMutation(
    Scene& scene) noexcept
{
    m_Context.selection.Validate(scene);
    m_Context.project->MarkDirty();
}

Result<void> EditorActions::ExecutePrepared(
    Scene& scene,
    std::unique_ptr<ICommand> command)
{
    if (m_Context.project == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Editor action requires an open project.");
    }

    auto executed =
        m_Context.project->GetCommandBus().Execute(
            std::move(command));
    if (!executed)
    {
        return executed;
    }

    FinishSuccessfulMutation(scene);
    return Result<void>::Success();
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

    const UUID id = UUID::Random();
    Scene& scene = *editable.Value();

    auto executed = ExecutePrepared(
        scene,
        std::make_unique<CreateEntityCommand>(
            scene,
            id,
            std::move(name)));
    if (!executed)
    {
        return Result<UUID>::Failure(
            executed.GetError());
    }

    m_Context.selection.Select(id);
    return Result<UUID>::Success(id);
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
    SceneReflection reflection(
        m_Context.project->GetReflectionRegistry(),
        &m_Context.project->GetAssetRegistry());

    return ExecutePrepared(
        scene,
        std::make_unique<DeleteEntityCommand>(
            scene,
            reflection,
            id));
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

    Scene& scene = *editable.Value();
    return ExecutePrepared(
        scene,
        std::make_unique<RenameEntityCommand>(
            scene,
            id,
            std::move(name)));
}

Result<void> EditorActions::SetProperty(
    UUID id,
    ComponentTypeId component,
    PropertyId property,
    PropertyValue value)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    SceneReflection reflection(
        m_Context.project->GetReflectionRegistry(),
        &m_Context.project->GetAssetRegistry());

    return ExecutePrepared(
        scene,
        std::make_unique<SetPropertyCommand>(
            scene,
            reflection,
            id,
            component,
            property,
            std::move(value)));
}

Result<void> EditorActions::AddComponent(
    UUID id,
    ComponentTypeId component)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    SceneReflection reflection(
        m_Context.project->GetReflectionRegistry(),
        &m_Context.project->GetAssetRegistry());

    return ExecutePrepared(
        scene,
        std::make_unique<AddComponentCommand>(
            scene,
            reflection,
            id,
            component));
}

Result<void> EditorActions::RemoveComponent(
    UUID id,
    ComponentTypeId component)
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    Scene& scene = *editable.Value();
    SceneReflection reflection(
        m_Context.project->GetReflectionRegistry(),
        &m_Context.project->GetAssetRegistry());

    return ExecutePrepared(
        scene,
        std::make_unique<RemoveComponentCommand>(
            scene,
            reflection,
            id,
            component));
}

Result<void> EditorActions::Undo()
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    auto undone =
        m_Context.project->GetCommandBus().Undo();
    if (!undone)
    {
        return undone;
    }

    FinishSuccessfulMutation(*editable.Value());
    return Result<void>::Success();
}

Result<void> EditorActions::Redo()
{
    auto editable = GetEditableScene();
    if (!editable)
    {
        return Result<void>::Failure(
            editable.GetError());
    }

    auto redone =
        m_Context.project->GetCommandBus().Redo();
    if (!redone)
    {
        return redone;
    }

    FinishSuccessfulMutation(*editable.Value());
    return Result<void>::Success();
}

bool EditorActions::CanUndo() const noexcept
{
    return m_Context.project != nullptr
        && !m_Context.project->IsPlaying()
        && m_Context.project->GetCommandBus().CanUndo();
}

bool EditorActions::CanRedo() const noexcept
{
    return m_Context.project != nullptr
        && !m_Context.project->IsPlaying()
        && m_Context.project->GetCommandBus().CanRedo();
}

Result<void> EditorActions::SetTransform(
    UUID id,
    Vector2 position,
    f32 rotationRadians,
    Vector2 scale)
{
    auto result = SetProperty(
        id,
        SceneReflectionIds::Transform,
        SceneReflectionIds::TransformPosition,
        PropertyValue{position});
    if (!result)
    {
        return result;
    }

    result = SetProperty(
        id,
        SceneReflectionIds::Transform,
        SceneReflectionIds::TransformRotation,
        PropertyValue{rotationRadians});
    if (!result)
    {
        return result;
    }

    return SetProperty(
        id,
        SceneReflectionIds::Transform,
        SceneReflectionIds::TransformScale,
        PropertyValue{scale});
}

Result<void> EditorActions::AddSpriteRenderer(UUID id)
{
    return AddComponent(
        id,
        SceneReflectionIds::SpriteRenderer);
}

Result<void> EditorActions::RemoveSpriteRenderer(UUID id)
{
    return RemoveComponent(
        id,
        SceneReflectionIds::SpriteRenderer);
}

Result<void> EditorActions::SetSpriteRenderer(
    UUID id,
    Vector2 size,
    Color color,
    i32 layer,
    bool enabled)
{
    auto result = SetProperty(
        id,
        SceneReflectionIds::SpriteRenderer,
        SceneReflectionIds::SpriteSize,
        PropertyValue{size});
    if (!result)
    {
        return result;
    }

    result = SetProperty(
        id,
        SceneReflectionIds::SpriteRenderer,
        SceneReflectionIds::SpriteColor,
        PropertyValue{
            ColorValue{
                color.r,
                color.g,
                color.b,
                color.a}});
    if (!result)
    {
        return result;
    }

    result = SetProperty(
        id,
        SceneReflectionIds::SpriteRenderer,
        SceneReflectionIds::SpriteLayer,
        PropertyValue{layer});
    if (!result)
    {
        return result;
    }

    return SetProperty(
        id,
        SceneReflectionIds::SpriteRenderer,
        SceneReflectionIds::SpriteEnabled,
        PropertyValue{enabled});
}

Result<void> EditorActions::SetSpriteTexture(
    UUID id,
    AssetHandle texture)
{
    return SetProperty(
        id,
        SceneReflectionIds::SpriteRenderer,
        SceneReflectionIds::SpriteTexture,
        PropertyValue{
            AssetReferenceValue{texture.id}});
}

Result<void> EditorActions::AddCamera(UUID id)
{
    return AddComponent(
        id,
        SceneReflectionIds::Camera);
}

Result<void> EditorActions::RemoveCamera(UUID id)
{
    return RemoveComponent(
        id,
        SceneReflectionIds::Camera);
}

Result<void> EditorActions::SetCamera(
    UUID id,
    f32 zoom,
    bool primary)
{
    auto result = SetProperty(
        id,
        SceneReflectionIds::Camera,
        SceneReflectionIds::CameraZoom,
        PropertyValue{zoom});
    if (!result)
    {
        return result;
    }

    return SetProperty(
        id,
        SceneReflectionIds::Camera,
        SceneReflectionIds::CameraPrimary,
        PropertyValue{primary});
}

Result<void> EditorActions::AddLuaScript(UUID id)
{
    return AddComponent(
        id,
        SceneReflectionIds::LuaScript);
}

Result<void> EditorActions::RemoveLuaScript(UUID id)
{
    return RemoveComponent(
        id,
        SceneReflectionIds::LuaScript);
}

Result<void> EditorActions::SetLuaScriptEnabled(
    UUID id,
    bool enabled)
{
    return SetProperty(
        id,
        SceneReflectionIds::LuaScript,
        SceneReflectionIds::LuaScriptEnabled,
        PropertyValue{enabled});
}

Result<void> EditorActions::SetLuaScriptAsset(
    UUID id,
    AssetHandle script)
{
    return SetProperty(
        id,
        SceneReflectionIds::LuaScript,
        SceneReflectionIds::LuaScriptAsset,
        PropertyValue{
            AssetReferenceValue{script.id}});
}

} // namespace Janus::Editor
