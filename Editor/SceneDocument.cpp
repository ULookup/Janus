#include "Core/FileSystem/FileSystem.h"
#include "ProjectSession.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneSerializer.h"

namespace Janus::Editor
{
namespace
{
Result<void> ValidateSceneAssets(const Scene& scene, const ReflectionRegistry& reflection,
                                 const AssetRegistry& assets)
{
    SceneReflection access(reflection);
    for (const auto entity : scene.GetEntities())
    {
        const auto id = scene.GetComponent<EntityIdentityComponent>(entity)->id;
        for (const auto* component : reflection.GetComponents())
        {
            auto present = access.HasComponent(scene, id, component->id);
            if (!present)
                return Result<void>::Failure(present.GetError());
            if (!present.Value())
                continue;
            for (const auto& property : component->properties)
            {
                if (!property.serializable || property.type != PropertyType::AssetReference)
                    continue;
                auto value = access.GetProperty(scene, id, component->id, property.id);
                if (!value)
                    return Result<void>::Failure(value.GetError());
                const auto* reference = std::get_if<AssetReferenceValue>(&value.Value());
                if (!reference || !reference->id.IsValid())
                    continue;
                const auto* asset = assets.Find(AssetHandle{reference->id});
                if (!asset || (!property.referenceConstraint.empty() &&
                               AssetTypeName(asset->type) != property.referenceConstraint))
                    return Result<void>::Failure(
                        ErrorCode::InvalidArgument,
                        "Scene references an unknown asset or incompatible type: " +
                            reference->id.ToString());
            }
        }
    }
    return Result<void>::Success();
}
} // namespace
PreparedScene::~PreparedScene() = default;

Result<std::filesystem::path>
ProjectSession::ResolveScenePath(const std::filesystem::path& path) const
{
    auto resolved = ResolveProjectPath(m_ProjectRoot, path);
    if (!resolved)
        return resolved;
    std::error_code error;
    if (path.extension() != ".scene" ||
        !std::filesystem::is_directory(resolved.Value().parent_path(), error) || error)
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            "Choose a .scene path in an existing project directory; create the directory first.");
    const bool exists = std::filesystem::exists(resolved.Value(), error);
    if (error || (exists && !std::filesystem::is_regular_file(resolved.Value(), error)) || error)
        return Result<std::filesystem::path>::Failure(ErrorCode::InvalidArgument,
                                                      "Scene path is not a regular file.");
    return resolved;
}

Result<std::unique_ptr<PreparedScene>>
ProjectSession::PrepareScene(const std::filesystem::path& path, bool create)
{
    using Return = Result<std::unique_ptr<PreparedScene>>;
    if (std::this_thread::get_id() != m_OwnerThread)
        return Return::Failure(ErrorCode::InvalidState,
                               "Scene preparation requires the owner thread.");
    auto resolved = ResolveScenePath(path);
    if (!resolved)
        return Return::Failure(resolved.GetError());
    auto candidate = std::unique_ptr<PreparedScene>(new PreparedScene);
    candidate->m_Project = m_ProjectIdentity;
    candidate->m_Revision = m_SceneRevision;
    candidate->m_Generation = m_AuthoringGeneration;
    candidate->m_Path = path.lexically_normal();
    candidate->m_New = create;
    if (create)
    {
        if (FileSystem::Exists(resolved.Value()))
            return Return::Failure(ErrorCode::InvalidArgument, "New scene path already exists.");
        candidate->m_Scene = std::make_unique<Scene>();
        candidate->m_Scene->SetName(FileSystem::PathToUtf8(path.stem()));
    }
    else
    {
        auto source = FileSystem::ReadText(resolved.Value());
        if (!source)
            return Return::Failure(source.GetError());
        auto loaded = SceneDeserializer::Deserialize(source.Value(), m_ReflectionRegistry);
        if (!loaded)
            return Return::Failure(loaded.GetError());
        auto assets = ValidateSceneAssets(*loaded.Value(), m_ReflectionRegistry, m_AssetRegistry);
        if (!assets)
            return Return::Failure(assets.GetError());
        candidate->m_Source = std::move(source).Value();
        candidate->m_Scene = std::move(loaded).Value();
    }
    return Return::Success(std::move(candidate));
}

Result<std::unique_ptr<PreparedScene>>
ProjectSession::PrepareNewScene(const std::filesystem::path& path)
{
    return PrepareScene(path, true);
}
Result<std::unique_ptr<PreparedScene>>
ProjectSession::PrepareOpenScene(const std::filesystem::path& path)
{
    return PrepareScene(path, false);
}
Result<void> ProjectSession::ValidatePreparedScene(const PreparedScene& candidate) const
{
    if (std::this_thread::get_id() != m_OwnerThread || !candidate.m_Scene ||
        candidate.m_Project != m_ProjectIdentity || candidate.m_Revision != m_SceneRevision ||
        candidate.m_Generation != m_AuthoringGeneration)
        return Result<void>::Failure(ErrorCode::InvalidState,
                                     "Scene context changed; cancel and prepare the scene again.");
    auto resolved = ResolveScenePath(candidate.m_Path);
    if (!resolved)
        return Result<void>::Failure(resolved.GetError());
    if (candidate.m_New)
    {
        if (FileSystem::Exists(resolved.Value()))
            return Result<void>::Failure(ErrorCode::InvalidState,
                                         "New scene path is now occupied.");
    }
    else
    {
        auto source = FileSystem::ReadText(resolved.Value());
        if (!source)
            return Result<void>::Failure(source.GetError());
        if (source.Value() != candidate.m_Source)
            return Result<void>::Failure(ErrorCode::InvalidState,
                                         "Scene file changed; cancel and prepare it again.");
    }
    return ValidateSceneAssets(*candidate.m_Scene, m_ReflectionRegistry, m_AssetRegistry);
}
void ProjectSession::ReplacePreparedScene(PreparedScene& candidate)
{
    // Commands borrow the old Scene. Release them before the no-fail ownership swap.
    m_CommandBus.ResetAfterRecovery();
    m_EditorScene.swap(candidate.m_Scene);
    m_CurrentScenePath.swap(candidate.m_Path);
    m_HasSavedFile = !candidate.m_New;
    m_Dirty = candidate.m_New;
    m_TransactionOwner = {};
    m_TransactionOwners.clear();
    ++m_SceneRevision;
    ++m_AuthoringGeneration;
    candidate.m_Scene.reset();
}
Result<void> ProjectSession::CommitPreparedScene(PreparedScene& candidate)
{
    if (IsAuthoringReadOnly() || IsDirty())
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Save the scene and stop Runtime/transactions before changing documents.");
    auto valid = ValidatePreparedScene(candidate);
    if (!valid)
        return valid;
    ReplacePreparedScene(candidate);
    return Result<void>::Success();
}
Result<void> ProjectSession::NewScene(const std::filesystem::path& path)
{
    auto candidate = PrepareNewScene(path);
    if (!candidate)
        return Result<void>::Failure(candidate.GetError());
    return CommitPreparedScene(*candidate.Value());
}
Result<void> ProjectSession::OpenScene(const std::filesystem::path& path)
{
    auto candidate = PrepareOpenScene(path);
    if (!candidate)
        return Result<void>::Failure(candidate.GetError());
    return CommitPreparedScene(*candidate.Value());
}
Result<void> ProjectSession::SaveSceneAs(const std::filesystem::path& path, bool overwrite)
{
    if (std::this_thread::get_id() != m_OwnerThread || IsAuthoringReadOnly())
        return Result<void>::Failure(ErrorCode::InvalidState, "Scene saving is unavailable.");
    auto resolved = ResolveScenePath(path);
    if (!resolved)
        return Result<void>::Failure(resolved.GetError());
    auto destination = path.lexically_normal();
    auto serialized = SceneSerializer::Serialize(*m_EditorScene, m_ReflectionRegistry);
    if (!serialized)
        return Result<void>::Failure(serialized.GetError());
    auto saved = FileSystem::WriteTextAtomic(resolved.Value(), serialized.Value(),
                                             overwrite ? FileSystem::AtomicWriteMode::Replace
                                                       : FileSystem::AtomicWriteMode::CreateNew);
    if (!saved)
        return saved;
    m_CurrentScenePath.swap(destination);
    m_HasSavedFile = true;
    m_Dirty = false;
    ++m_AuthoringGeneration;
    return Result<void>::Success();
}
} // namespace Janus::Editor
