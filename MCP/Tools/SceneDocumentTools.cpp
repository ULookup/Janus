#include "Tools/SceneDocumentTools.h"

namespace Janus::MCP
{
Result<void> RegisterSceneDocumentTools(ToolRegistry& tools, SceneDocumentToolContext context)
{
    if (!context.revision || !context.execute)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Scene document callbacks required.");
    for (const std::string name : {"scene.new", "scene.open", "scene.save_as"})
    {
        McpToolDescriptor tool;
        tool.name = name;
        tool.title = name;
        tool.description =
            "Change the active scene document outside Undo/transactions. New/Open require a saved, "
            "stopped scene. Paths must use an existing project directory.";
        Json properties{{"path", {{"type", "string"}, {"minLength", 1}, {"maxLength", 1024}}},
                        {"expectedRevision", {{"type", "integer"}, {"minimum", 0}}}};
        if (name == "scene.save_as")
            properties["overwrite"] = {{"type", "boolean"}, {"default", false}};
        tool.inputSchema = {{"type", "object"},
                            {"properties", properties},
                            {"required", Json::array({"path"})},
                            {"additionalProperties", false}};
        tool.handler = [context, name](const Json& args, McpProtocolEra) -> McpDispatchResult
        {
            if (!args.is_object() || !args.contains("path") || !args["path"].is_string())
                return McpDispatchError{JsonRpcInvalidParams,
                                        "A project-relative scene path is required.", nullptr};
            for (const auto& [key, value] : args.items())
                if (key != "path" && key != "expectedRevision" &&
                    !(name == "scene.save_as" && key == "overwrite"))
                    return McpDispatchError{
                        JsonRpcInvalidParams,
                        "Scene lifecycle does not accept transactions or unknown parameters.",
                        nullptr};
            if (args.contains("overwrite") && !args["overwrite"].is_boolean())
                return McpDispatchError{JsonRpcInvalidParams, "overwrite must be boolean.",
                                        nullptr};
            if (args.contains("expectedRevision") &&
                (!args["expectedRevision"].is_number_unsigned() &&
                 !(args["expectedRevision"].is_number_integer() &&
                   args["expectedRevision"].get<i64>() >= 0)))
                return McpDispatchError{JsonRpcInvalidParams,
                                        "expectedRevision must be non-negative integer.", nullptr};
            if (args.contains("expectedRevision") &&
                args["expectedRevision"].get<u64>() != context.revision())
                return McpDispatchError{JsonRpcInvalidParams,
                                        "Scene revision changed; read the current context.",
                                        nullptr};
            const auto result = context.execute(name, args["path"].get_ref<const std::string&>(),
                                                args.value("overwrite", false));
            Json payload{{"ok", static_cast<bool>(result)}, {"sceneRevision", context.revision()}};
            if (!result)
                payload["error"] = {{"code", static_cast<i32>(result.GetError().code)},
                                    {"message", result.GetError().message}};
            return Json{{"isError", !result},
                        {"structuredContent", payload},
                        {"content", Json::array({{{"type", "text"}, {"text", payload.dump()}}})}};
        };
        auto registered = tools.RegisterTool(std::move(tool));
        if (!registered)
            return registered;
    }
    return Result<void>::Success();
}
} // namespace Janus::MCP
