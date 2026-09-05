#pragma once

#include "Core/Error/Result.h"
#include "Protocol/McpProtocol.h"

#include <string>
#include <string_view>

namespace Janus::MCP
{

enum class McpOperation
{
    ProjectRead,
    SceneRead,
    SceneWrite,
    SceneSave
};

struct McpRequestContext
{
    std::string method;
    std::string target;
    McpProtocolEra era =
        McpProtocolEra::Unspecified;
};

class IMcpPermissionPolicy
{
public:
    virtual ~IMcpPermissionPolicy() = default;

    [[nodiscard]] virtual Result<void> Authorize(
        McpOperation operation,
        const McpRequestContext& request) const = 0;
};

class AllowAllMcpPermissionPolicy final
    : public IMcpPermissionPolicy
{
public:
    [[nodiscard]] Result<void> Authorize(
        McpOperation,
        const McpRequestContext&) const override;
};

[[nodiscard]] McpOperation ClassifyMcpOperation(
    std::string_view method,
    const Json& params) noexcept;

[[nodiscard]] std::string McpRequestTarget(
    std::string_view method,
    const Json& params);

} // namespace Janus::MCP
