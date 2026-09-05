#pragma once

#include "Core/Error/Result.h"
#include "Registry/ToolRegistry.h"

#include <functional>

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
};

[[nodiscard]] Result<void> RegisterSceneTools(
    ToolRegistry& registry,
    McpSceneToolContext context);

} // namespace MCP
} // namespace Janus
