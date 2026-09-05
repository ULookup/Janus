#pragma once

#include "Protocol/McpProtocol.h"
#include "Registry/ResourceRegistry.h"
#include "Registry/ToolRegistry.h"

#include <string_view>

namespace Janus::MCP
{

class McpCapabilityRouter final
{
public:
    McpCapabilityRouter(
        ToolRegistry& tools,
        ResourceRegistry& resources) noexcept;

    [[nodiscard]] McpDispatchResult HandleRequest(
        std::string_view method,
        const Json& params,
        McpProtocolEra era) const;

private:
    ToolRegistry& m_Tools;
    ResourceRegistry& m_Resources;
};

} // namespace Janus::MCP
