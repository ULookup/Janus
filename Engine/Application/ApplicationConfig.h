#pragma once

#include "Core/Log/LogOutput.h"
#include "Core/Time/TimeStep.h"
#include "Platform/Window/WindowConfig.h"

#include <filesystem>
#include <optional>

namespace Janus
{

enum class ApplicationExecutionMode
{
    ManagedRuntime,
    ClientDriven
};

struct ProjectRuntimeConfig
{
    std::filesystem::path root;
    // Explicit overrides win over project.json; omitted values use manifest/legacy defaults.
    std::optional<std::filesystem::path> assetRegistryPath;
    std::optional<std::filesystem::path> startupScenePath;
};

struct ApplicationConfig
{
    WindowConfig window;
    TimeStep maximumFrameTime = TimeStep::FromMilliseconds(250.0);
    ApplicationExecutionMode executionMode =
        ApplicationExecutionMode::ManagedRuntime;
    LogOutput logOutput =
        LogOutput::StandardOutput;
    std::optional<ProjectRuntimeConfig> project;
    bool vsync = true;
    u32 targetFps = 0;
};

} // namespace Janus
