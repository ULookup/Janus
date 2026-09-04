#pragma once

#include "Application/ApplicationConfig.h"
#include "Asset/AssetRegistry.h"
#include "Core/Error/Result.h"

#include <filesystem>
#include <memory>

namespace Janus
{

class AssetService;
class Renderer2D;
class Scene;

namespace Editor
{

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
    [[nodiscard]] AssetService& GetAssetService() noexcept;
    [[nodiscard]] Scene& GetEditorScene() noexcept;
    [[nodiscard]] const Scene& GetEditorScene() const noexcept;

private:
    ProjectSession(
        std::filesystem::path projectRoot,
        std::filesystem::path currentScenePath,
        AssetRegistry assetRegistry,
        std::unique_ptr<Scene> editorScene,
        Renderer2D& renderer);

    std::filesystem::path m_ProjectRoot;
    std::filesystem::path m_CurrentScenePath;
    AssetRegistry m_AssetRegistry;
    std::unique_ptr<AssetService> m_AssetService;
    std::unique_ptr<Scene> m_EditorScene;
};

} // namespace Editor
} // namespace Janus
