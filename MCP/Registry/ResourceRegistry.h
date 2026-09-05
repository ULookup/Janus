#pragma once

#include "Core/Error/Result.h"
#include "Protocol/McpProtocol.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Janus::MCP
{

using McpResourceReadHandler =
    std::function<McpDispatchResult(
        std::string_view uri,
        McpProtocolEra era)>;

struct McpResourceDescriptor
{
    std::string uri;
    std::string name;
    std::string title;
    std::string description;
    std::string mimeType;
    McpResourceReadHandler handler;
};

struct McpResourceTemplateDescriptor
{
    std::string uriTemplate;
    std::string name;
    std::string title;
    std::string description;
    std::string mimeType;
    McpResourceReadHandler handler;
};

class ResourceRegistry final
{
public:
    [[nodiscard]] Result<void> RegisterResource(
        McpResourceDescriptor descriptor);

    [[nodiscard]] Result<void> RegisterTemplate(
        McpResourceTemplateDescriptor descriptor);

    [[nodiscard]] const McpResourceDescriptor* FindResource(
        std::string_view uri) const noexcept;

    [[nodiscard]] const McpResourceTemplateDescriptor* FindTemplate(
        std::string_view uriTemplate) const noexcept;

    [[nodiscard]] const McpResourceTemplateDescriptor*
    MatchTemplate(
        std::string_view uri) const noexcept;

    [[nodiscard]] std::vector<const McpResourceDescriptor*> GetResources()
        const;

    [[nodiscard]] std::vector<const McpResourceTemplateDescriptor*>
    GetTemplates() const;

    [[nodiscard]] McpDispatchResult HandleList(
        const Json& params,
        McpProtocolEra era) const;

    [[nodiscard]] McpDispatchResult HandleTemplatesList(
        const Json& params,
        McpProtocolEra era) const;

    [[nodiscard]] McpDispatchResult HandleRead(
        const Json& params,
        McpProtocolEra era) const;

    [[nodiscard]] usize GetResourceCount() const noexcept;
    [[nodiscard]] usize GetTemplateCount() const noexcept;

private:
    std::map<std::string, McpResourceDescriptor> m_Resources;
    std::map<std::string, McpResourceTemplateDescriptor> m_Templates;
};

} // namespace Janus::MCP
