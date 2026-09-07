#pragma once
#include "Core/Log/LogStore.h"
#include "Diagnostics/DiagnosticsFrame.h"
#include "Registry/ToolRegistry.h"
#include "Resources/SceneResources.h"
#include "Runtime/RuntimeSession.h"
#include <memory>
namespace Janus::MCP
{
struct McpDebugContext
{
    std::function<RuntimeStatus()> status;
    std::function<const Scene*()> scene;
    std::function<Result<void>(std::string_view, bool)> control;
    const ReflectionRegistry* reflection = nullptr;
    const AssetRegistry* assets = nullptr;
    std::shared_ptr<LogStore> logs;
    std::function<std::optional<DiagnosticsFrame>(std::optional<u64>)> profile;
    std::function<bool()> authoringReadOnly;
    std::function<std::optional<ScriptSnapshot>()> snapshot;
};
Json RuntimeStatusJson(const RuntimeStatus& status);
Json ResourceContent(std::string_view uri, const Json& payload);
Json OperationResult(const Result<void>& result);
Result<void> RegisterDebugCapabilities(ToolRegistry& tools, ResourceRegistry& resources,
                                       McpDebugContext context);
} // namespace Janus::MCP
