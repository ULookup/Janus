#pragma once

#include "Core/Error/Result.h"
#include "Registry/ToolRegistry.h"

#include "Core/Command/ICommand.h"
#include <functional>
#include <memory>

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
};

[[nodiscard]] Result<void> RegisterSceneTools(
    ToolRegistry& registry,
    McpSceneToolContext context);

} // namespace MCP
} // namespace Janus
