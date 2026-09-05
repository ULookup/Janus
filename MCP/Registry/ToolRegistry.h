#pragma once

#include "Core/Error/Result.h"
#include "Protocol/McpProtocol.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Janus::MCP
{

inline constexpr std::string_view McpJsonSchema202012 =
    "https://json-schema.org/draft/2020-12/schema";

using McpToolHandler =
    std::function<McpDispatchResult(
        const Json& arguments,
        McpProtocolEra era)>;

struct McpToolDescriptor
{
    std::string name;
    std::string title;
    std::string description;
    Json inputSchema = Json::object();
    std::optional<Json> outputSchema;
    Json annotations = Json::object();
    McpToolHandler handler;
};

class ToolRegistry final
{
public:
    [[nodiscard]] Result<void> RegisterTool(
        McpToolDescriptor descriptor);

    [[nodiscard]] const McpToolDescriptor* FindTool(
        std::string_view name) const noexcept;

    [[nodiscard]] std::vector<const McpToolDescriptor*> GetTools()
        const;

    [[nodiscard]] McpDispatchResult HandleList(
        const Json& params,
        McpProtocolEra era) const;

    [[nodiscard]] McpDispatchResult HandleCall(
        const Json& params,
        McpProtocolEra era) const;

    [[nodiscard]] usize GetToolCount() const noexcept;

private:
    std::map<std::string, McpToolDescriptor> m_Tools;
};

} // namespace Janus::MCP
