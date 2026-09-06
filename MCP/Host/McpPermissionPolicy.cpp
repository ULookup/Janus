#include "Host/McpPermissionPolicy.h"

namespace Janus::MCP
{

Result<void> AllowAllMcpPermissionPolicy::Authorize(McpOperation operation,
                                                    const McpRequestContext&) const
{
    if (operation == McpOperation::Unclassified)
        return Result<void>::Failure(ErrorCode::InvalidState, "Unclassified MCP operation denied.");
    return Result<void>::Success();
}

McpOperation ClassifyMcpOperation(std::string_view method, const Json& params) noexcept
{
    if (method == "tools/call")
    {
        auto it = params.find("name");
        if (it == params.end() || !it->is_string())
            return McpOperation::Unclassified;
        const std::string_view name = it->get_ref<const std::string&>();
        if (name == "scene.save")
            return McpOperation::SceneSave;
        for (auto known : {"scene.create_entity", "scene.delete_entity", "scene.rename_entity",
                           "scene.reparent_entity", "scene.add_component", "scene.remove_component",
                           "scene.set_component_property"})
            if (name == known)
                return McpOperation::SceneWrite;
        for (auto known : {"runtime.play", "runtime.pause", "runtime.stop", "runtime.step"})
            if (name == known)
                return McpOperation::RuntimeControl;
        for (auto known : {"transaction.begin", "transaction.commit", "transaction.rollback"})
            if (name == known)
                return McpOperation::TransactionControl;
        return McpOperation::Unclassified;
    }
    if (method == "resources/read")
    {
        auto it = params.find("uri");
        if (it == params.end() || !it->is_string())
            return McpOperation::Unclassified;
        std::string_view uri = it->get_ref<const std::string&>();
        uri = uri.substr(0, uri.find('?'));
        if (uri == "engine://runtime/status" || uri.starts_with("engine://runtime/entity/"))
            return McpOperation::RuntimeRead;
        if (uri == "engine://logs/recent" || uri == "engine://profiler/latest-frame")
            return McpOperation::DiagnosticsRead;
        if (uri == "engine://transaction/status" || uri == "engine://agent/activity")
            return McpOperation::ActivityRead;
        if (uri == "engine://scene/current" || uri == "engine://scene/hierarchy" ||
            uri.starts_with("engine://entity/"))
            return McpOperation::SceneRead;
        if (uri == "engine://project/info" || uri.starts_with("engine://asset/"))
            return McpOperation::ProjectRead;
        return McpOperation::Unclassified;
    }
    if (method == "tools/list" || method == "resources/list" ||
        method == "resources/templates/list")
        return McpOperation::ProjectRead;
    return McpOperation::Unclassified;
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
