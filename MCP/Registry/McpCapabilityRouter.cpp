#include "Registry/McpCapabilityRouter.h"

#include <string>

namespace Janus::MCP
{

McpCapabilityRouter::McpCapabilityRouter(
    ToolRegistry& tools,
    ResourceRegistry& resources) noexcept
    : m_Tools(tools),
      m_Resources(resources)
{
}

McpDispatchResult McpCapabilityRouter::HandleRequest(
    std::string_view method,
    const Json& params,
    McpProtocolEra era) const
{
    if (method == "tools/list")
    {
        return m_Tools.HandleList(
            params,
            era);
    }

    if (method == "tools/call")
    {
        return m_Tools.HandleCall(
            params,
            era);
    }

    if (method == "resources/list")
    {
        return m_Resources.HandleList(
            params,
            era);
    }

    if (method == "resources/templates/list")
    {
        return m_Resources.HandleTemplatesList(
            params,
            era);
    }

    if (method == "resources/read")
    {
        return m_Resources.HandleRead(
            params,
            era);
    }

    return McpDispatchError{
        JsonRpcMethodNotFound,
        "MCP method not found.",
        Json{{"method", std::string{method}}}};
}

} // namespace Janus::MCP
