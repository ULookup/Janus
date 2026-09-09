#pragma once
#include "Registry/ToolRegistry.h"
#include <functional>

namespace Janus::MCP
{
struct SceneDocumentToolContext
{
    std::function<u64()> revision;
    std::function<Result<void>(std::string_view, std::string_view, bool)> execute;
};
Result<void> RegisterSceneDocumentTools(ToolRegistry& tools, SceneDocumentToolContext context);
} // namespace Janus::MCP
