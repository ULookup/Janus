#pragma once
#include "Application/ApplicationConfig.h"
#include "Core/Input/InputActions.h"

namespace Janus
{
struct ProjectSettings
{
    std::string name = "Janus Project";
    std::filesystem::path defaultScene = "Scenes/Battle.scene";
    std::filesystem::path assetRegistry = "Config/AssetRegistry.json";
    std::filesystem::path assetRoot = "Assets";
    std::filesystem::path scriptRoot = "Scripts";
    u32 width = 1280;
    u32 height = 720;
    bool vsync = true;
    u32 targetFps = 0;
    InputBindings inputBindings;
    bool operator==(const ProjectSettings&) const = default;
};
[[nodiscard]] Result<std::filesystem::path>
ResolveProjectPath(const std::filesystem::path& root, const std::filesystem::path& relative);
[[nodiscard]] Result<void> ValidateProjectSettings(const std::filesystem::path& root,
                                                   const ProjectSettings& settings);
[[nodiscard]] Result<ProjectSettings> LoadProjectSettings(const ProjectRuntimeConfig& config);
[[nodiscard]] Result<void> SaveProjectSettings(const std::filesystem::path& root,
                                               const ProjectSettings& settings);
} // namespace Janus
