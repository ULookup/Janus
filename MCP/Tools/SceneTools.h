#pragma once

#include "Core/Error/Result.h"
#include "Registry/ToolRegistry.h"

#include "Asset/AssetHandle.h"
#include "Core/Command/ICommand.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Janus
{

class AssetRegistry;
class CommandBus;
class ReflectionRegistry;
class Scene;

namespace MCP
{

struct McpSceneToolContext
{
    Scene* scene = nullptr;
    const ReflectionRegistry* reflection = nullptr;
    CommandBus* commands = nullptr;
    const AssetRegistry* assets = nullptr;

    std::function<Result<void>()> saveCurrentScene;
    std::function<void()> markDirty;
    std::function<bool()> authoringReadOnly;
    std::function<Result<void>(std::unique_ptr<ICommand>, UUID)> executeCommand;
    std::filesystem::path projectRoot;
    std::function<Result<AssetHandle>(UUID, std::optional<std::string>)> exportPrefab;
};

[[nodiscard]] Result<void> RegisterSceneTools(
    ToolRegistry& registry,
    McpSceneToolContext context);

} // namespace MCP
} // namespace Janus
