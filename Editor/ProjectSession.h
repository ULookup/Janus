#pragma once

#include "Application/ApplicationConfig.h"
#include "Asset/AssetRegistry.h"
#include "Core/Command/CommandBus.h"
#include "Core/Error/Result.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/Time/TimeStep.h"

#include <filesystem>
#include <memory>

namespace Janus
{

class AssetService;
class InputState;
class Renderer2D;
class Scene;

namespace Editor
{

class RuntimeSession;

class ProjectSession final
{
public:
    [[nodiscard]] static Result<std::unique_ptr<ProjectSession>> Open(
        const ProjectRuntimeConfig& config,
        Renderer2D& renderer);

    ~ProjectSession();

    ProjectSession(const ProjectSession&) = delete;
    ProjectSession& operator=(const ProjectSession&) = delete;
    ProjectSession(ProjectSession&&) = delete;
    ProjectSession& operator=(ProjectSession&&) = delete;

    [[nodiscard]] const std::filesystem::path& GetProjectRoot() const noexcept;
    [[nodiscard]] const std::filesystem::path& GetCurrentScenePath() const noexcept;
    [[nodiscard]] const AssetRegistry& GetAssetRegistry() const noexcept;
    [[nodiscard]] ReflectionRegistry& GetReflectionRegistry() noexcept;
    [[nodiscard]] const ReflectionRegistry& GetReflectionRegistry() const noexcept;
    [[nodiscard]] CommandBus& GetCommandBus() noexcept;
    [[nodiscard]] const CommandBus& GetCommandBus() const noexcept;
    [[nodiscard]] AssetService& GetAssetService() noexcept;
    [[nodiscard]] Scene& GetEditorScene() noexcept;
    [[nodiscard]] const Scene& GetEditorScene() const noexcept;

    [[nodiscard]] Result<void> StartRuntime(const InputState& input);
    [[nodiscard]] Result<void> UpdateRuntime(TimeStep timeStep);
    [[nodiscard]] Result<void> StopRuntime();

    [[nodiscard]] bool IsPlaying() const noexcept;
    [[nodiscard]] bool IsDirty() const noexcept;
    void MarkDirty() noexcept;
    [[nodiscard]] Result<void> SaveCurrentScene();

    [[nodiscard]] RuntimeSession* GetRuntimeSession() noexcept;
    [[nodiscard]] const RuntimeSession* GetRuntimeSession() const noexcept;

private:
    ProjectSession(
        std::filesystem::path projectRoot,
        std::filesystem::path currentScenePath,
        ReflectionRegistry reflectionRegistry,
        AssetRegistry assetRegistry,
        std::unique_ptr<Scene> editorScene,
        Renderer2D& renderer);

    std::filesystem::path m_ProjectRoot;
    std::filesystem::path m_CurrentScenePath;
    ReflectionRegistry m_ReflectionRegistry;
    AssetRegistry m_AssetRegistry;
    std::unique_ptr<AssetService> m_AssetService;
    std::unique_ptr<Scene> m_EditorScene;

    // Commands retain references into the authoring capability graph.
    // Keep history after those dependencies in declaration order so it
    // is destroyed before Scene, AssetRegistry, and ReflectionRegistry.
    CommandBus m_CommandBus;
    std::unique_ptr<RuntimeSession> m_RuntimeSession;
    bool m_Dirty = false;
};

} // namespace Editor
} // namespace Janus
