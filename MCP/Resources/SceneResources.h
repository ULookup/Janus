#pragma once

#include "Core/Error/Result.h"
#include "Registry/ResourceRegistry.h"

#include <functional>
#include <string>

namespace Janus
{

class AssetRegistry;
class ReflectionRegistry;
class Scene;

namespace MCP
{

struct McpProjectReadState
{
    std::string projectDisplayPath;
    bool dirty = false;
    bool authoringReadOnly = false;
};

using McpProjectReadStateProvider =
    std::function<Result<McpProjectReadState>()>;

struct McpSceneResourceContext
{
    const Scene* scene = nullptr;
    const ReflectionRegistry* reflection = nullptr;
    const AssetRegistry* assets = nullptr;
    McpProjectReadStateProvider projectState;
};

[[nodiscard]] Result<void> RegisterSceneResources(
    ResourceRegistry& registry,
    McpSceneResourceContext context);

} // namespace MCP
} // namespace Janus
