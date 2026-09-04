#include "ProjectSession.h"

#include "RuntimeSession.h"

#include "Asset/AssetService.h"
#include "Core/Input/InputState.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneDeserializer.h"

#include <string>
#include <string_view>
#include <utility>

namespace Janus::Editor
{
namespace
{

Result<std::filesystem::path> ResolveProjectFile(
    const ProjectRuntimeConfig& project,
    const std::filesystem::path& relativePath,
    std::string_view label)
{
    if (project.root.empty())
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            "Project root must not be empty.");
    }

    if (relativePath.empty()
        || relativePath.is_absolute()
        || relativePath.has_root_name()
        || relativePath.has_root_directory())
    {
        return Result<std::filesystem::path>::Failure(
            ErrorCode::InvalidArgument,
            std::string(label) + " must be project-relative.");
    }

    const std::filesystem::path normalized = relativePath.lexically_normal();
    for (const auto& part : normalized)
    {
        if (part == std::filesystem::path(".."))
        {
            return Result<std::filesystem::path>::Failure(
                ErrorCode::InvalidArgument,
                std::string(label) + " cannot escape the project root.");
        }
    }

    return Result<std::filesystem::path>::Success(
        project.root / normalized);
}

} // namespace

Result<std::unique_ptr<ProjectSession>> ProjectSession::Open(
    const ProjectRuntimeConfig& config,
    Renderer2D& renderer)
{
    auto registryPath = ResolveProjectFile(
        config,
        config.assetRegistryPath,
        "Asset registry path");
    if (!registryPath)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            registryPath.GetError());
    }

    auto scenePath = ResolveProjectFile(
        config,
        config.startupScenePath,
        "Startup Scene path");
    if (!scenePath)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            scenePath.GetError());
    }

    auto registry = AssetRegistry::Load(registryPath.Value());
    if (!registry)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            registry.GetError());
    }

    auto scene = SceneDeserializer::Load(scenePath.Value());
    if (!scene)
    {
        return Result<std::unique_ptr<ProjectSession>>::Failure(
            scene.GetError());
    }

    auto session = std::unique_ptr<ProjectSession>(
        new ProjectSession(
            config.root,
            config.startupScenePath.lexically_normal(),
            std::move(registry).Value(),
            std::move(scene).Value(),
            renderer));

    return Result<std::unique_ptr<ProjectSession>>::Success(
        std::move(session));
}

ProjectSession::ProjectSession(
    std::filesystem::path projectRoot,
    std::filesystem::path currentScenePath,
    AssetRegistry assetRegistry,
    std::unique_ptr<Scene> editorScene,
    Renderer2D& renderer)
    : m_ProjectRoot(std::move(projectRoot)),
      m_CurrentScenePath(std::move(currentScenePath)),
      m_AssetRegistry(std::move(assetRegistry)),
      m_AssetService(std::make_unique<AssetService>(
          m_ProjectRoot,
          m_AssetRegistry,
          renderer)),
      m_EditorScene(std::move(editorScene))
{
}

ProjectSession::~ProjectSession() = default;

const std::filesystem::path& ProjectSession::GetProjectRoot() const noexcept
{
    return m_ProjectRoot;
}

const std::filesystem::path& ProjectSession::GetCurrentScenePath() const noexcept
{
    return m_CurrentScenePath;
}

const AssetRegistry& ProjectSession::GetAssetRegistry() const noexcept
{
    return m_AssetRegistry;
}

AssetService& ProjectSession::GetAssetService() noexcept
{
    return *m_AssetService;
}

Scene& ProjectSession::GetEditorScene() noexcept
{
    return *m_EditorScene;
}

const Scene& ProjectSession::GetEditorScene() const noexcept
{
    return *m_EditorScene;
}

Result<void> ProjectSession::StartRuntime(const InputState& input)
{
    if (m_RuntimeSession != nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ProjectSession already has an active RuntimeSession.");
    }

    auto runtime = RuntimeSession::Start(
        *m_EditorScene,
        *m_AssetService,
        input);
    if (!runtime)
    {
        return Result<void>::Failure(runtime.GetError());
    }

    m_RuntimeSession = std::move(runtime).Value();
    return Result<void>::Success();
}

Result<void> ProjectSession::UpdateRuntime(TimeStep timeStep)
{
    if (m_RuntimeSession == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "ProjectSession has no active RuntimeSession.");
    }

    return m_RuntimeSession->Update(timeStep);
}

Result<void> ProjectSession::StopRuntime()
{
    if (m_RuntimeSession == nullptr)
    {
        return Result<void>::Success();
    }

    auto stopped = m_RuntimeSession->Stop();
    m_RuntimeSession.reset();
    return stopped;
}

bool ProjectSession::IsPlaying() const noexcept
{
    return m_RuntimeSession != nullptr
        && m_RuntimeSession->IsRunning();
}

RuntimeSession* ProjectSession::GetRuntimeSession() noexcept
{
    return m_RuntimeSession.get();
}

const RuntimeSession* ProjectSession::GetRuntimeSession() const noexcept
{
    return m_RuntimeSession.get();
}

} // namespace Janus::Editor
