#include "Host/McpPermissionPolicy.h"

namespace Janus::MCP
{

Result<void> AllowAllMcpPermissionPolicy::Authorize(
    McpOperation,
    const McpRequestContext&) const
{
    return Result<void>::Success();
}

McpOperation ClassifyMcpOperation(
    std::string_view method,
    const Json& params) noexcept
{
    if (method == "tools/call")
    {
        const auto nameIt =
            params.find("name");

        if (nameIt != params.end()
            && nameIt->is_string())
        {
            const std::string_view name =
                nameIt->get_ref<const std::string&>();

            if (name == "scene.save")
            {
                return McpOperation::SceneSave;
            }

            if (name.starts_with("scene."))
            {
                return McpOperation::SceneWrite;
            }
        }

        return McpOperation::ProjectRead;
    }

    if (method == "resources/read")
    {
        const auto uriIt =
            params.find("uri");

        if (uriIt != params.end()
            && uriIt->is_string())
        {
            const std::string_view uri =
                uriIt->get_ref<const std::string&>();

            if (uri.starts_with("engine://scene/")
                || uri.starts_with("engine://entity/"))
            {
                return McpOperation::SceneRead;
            }
        }
    }

    return McpOperation::ProjectRead;
}

std::string McpRequestTarget(
    std::string_view method,
    const Json& params)
{
    if (method == "tools/call")
    {
        const auto nameIt =
            params.find("name");

        if (nameIt != params.end()
            && nameIt->is_string())
        {
            return nameIt->get<std::string>();
        }
    }

    if (method == "resources/read")
    {
        const auto uriIt =
            params.find("uri");

        if (uriIt != params.end()
            && uriIt->is_string())
        {
            return uriIt->get<std::string>();
        }
    }

    return std::string{method};
}

} // namespace Janus::MCP
