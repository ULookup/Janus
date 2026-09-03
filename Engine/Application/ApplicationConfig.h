#pragma once

#include "Core/Time/TimeStep.h"
#include "Platform/Window/WindowConfig.h"

#include <filesystem>
#include <optional>

namespace Janus
{

struct ProjectRuntimeConfig
{
    std::filesystem::path root;
    std::filesystem::path assetRegistryPath = "Config/AssetRegistry.json";
    std::filesystem::path startupScenePath = "Scenes/Battle.scene";
};

struct ApplicationConfig
{
    WindowConfig window;
    TimeStep maximumFrameTime = TimeStep::FromMilliseconds(250.0);
    std::optional<ProjectRuntimeConfig> project;
};

} // namespace Janus
