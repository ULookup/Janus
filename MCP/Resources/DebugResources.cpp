#include "Resources/DebugResources.h"
#include <charconv>
#include <map>
namespace Janus::MCP
{
namespace
{
McpDispatchError Invalid(std::string message)
{
    return {JsonRpcInvalidParams, std::move(message), nullptr};
}
Result<std::map<std::string, std::string>> Query(std::string_view uri)
{
    std::map<std::string, std::string> values;
    auto pos = uri.find('?');
    if (pos == std::string_view::npos)
        return Result<decltype(values)>::Success(std::move(values));
    uri.remove_prefix(pos + 1);
    while (!uri.empty())
    {
        const auto end = uri.find('&');
        const auto pair = uri.substr(0, end);
        const auto eq = pair.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq + 1 == pair.size() ||
            !values.emplace(std::string(pair.substr(0, eq)), std::string(pair.substr(eq + 1)))
                 .second)
            return Result<decltype(values)>::Failure(ErrorCode::InvalidArgument,
                                                     "Malformed or duplicate query parameter.");
        if (end == std::string_view::npos)
            break;
        uri.remove_prefix(end + 1);
    }
    return Result<decltype(values)>::Success(std::move(values));
}
bool Number(std::string_view value, u64& number)
{
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
} // namespace
Json RuntimeStatusJson(const RuntimeStatus& s)
{
    Json value = {
        {"state", RuntimeStateName(s.state)},
        {"runtimeId", s.runtimeId.IsValid() ? Json(s.runtimeId.ToString()) : Json(nullptr)},
        {"frameIndex", s.frameIndex},
        {"simulationTimeSeconds", s.simulationTimeSeconds},
        {"lastStepSeconds", s.lastStepSeconds},
        {"entityCount", s.entityCount},
        {"scriptInstanceCount", s.scriptInstanceCount},
        {"failedFrameIndex", s.failedFrameIndex},
        {"partialUpdate", s.partialUpdate},
        {"authoringReadOnly", s.state != RuntimeState::Stopped},
        {"lastError", nullptr}};
    if (s.lastError)
        value["lastError"] = {{"code", static_cast<i32>(s.lastError->code)},
                              {"message", s.lastError->message}};
    return value;
}
Json ResourceContent(std::string_view uri, const Json& payload)
{
    return {
        {"contents",
         Json::array({{{"uri", uri},
                       {"mimeType", "application/json"},
                       {"text", payload.dump(-1, ' ', false, Json::error_handler_t::replace)}}})}};
}
Json OperationResult(const Result<void>& result)
{
    Json data = {{"ok", static_cast<bool>(result)}};
    if (!result)
        data["error"] = {{"code", static_cast<i32>(result.GetError().code)},
                         {"message", result.GetError().message}};
    return {{"structuredContent", data},
            {"content", Json::array({{{"type", "text"}, {"text", data.dump()}}})},
            {"isError", !result}};
}
namespace
{
Json RuntimePayload(const McpDebugContext& context)
{
    auto payload = RuntimeStatusJson(context.status());
    if (context.authoringReadOnly)
        payload["authoringReadOnly"] = context.authoringReadOnly();
    return payload;
}
} // namespace

Result<void> RegisterDebugCapabilities(ToolRegistry& tools, ResourceRegistry& resources,
                                       McpDebugContext context)
{
    if (!context.status || !context.scene || !context.control || !context.reflection ||
        !context.assets || !context.logs)
        return Result<void>::Failure(ErrorCode::InvalidArgument,
                                     "Debug capabilities require complete host callbacks.");
    for (std::string name : {"runtime.play", "runtime.pause", "runtime.stop", "runtime.step"})
    {
        Json properties = Json::object();
        if (name == "runtime.play")
            properties["startPaused"] = {{"type", "boolean"}};
        McpToolDescriptor tool;
        tool.name = name;
        tool.title = name;
        tool.description =
            "Control the live Engine runtime. Step uses neutral input and 1/60 second.";
        tool.inputSchema = {{"$schema", McpJsonSchema202012},
                            {"type", "object"},
                            {"properties", properties},
                            {"additionalProperties", false}};
        tool.handler = [context, name](const Json& args, McpProtocolEra) -> McpDispatchResult
        {
            if (!args.is_object())
                return Invalid("Runtime arguments must be an object.");
            for (auto it = args.begin(); it != args.end(); ++it)
                if (name != "runtime.play" || it.key() != "startPaused" || !it->is_boolean())
                    return Invalid("Unknown or invalid runtime argument.");
            auto result = name == "runtime.play" && args.contains("startPaused") &&
                                  context.status().state != RuntimeState::Stopped
                              ? Result<void>::Failure(ErrorCode::InvalidState,
                                                      "startPaused is only valid when stopped.")
                              : context.control(name, args.value("startPaused", false));
            auto response = OperationResult(result);
            response["structuredContent"]["runtime"] = RuntimePayload(context);
            response["content"][0]["text"] = response["structuredContent"].dump();
            return response;
        };
        auto added = tools.RegisterTool(std::move(tool));
        if (!added)
            return added;
    }
    auto added = resources.RegisterResource(
        {"engine://runtime/status", "runtime-status", "Runtime status",
         "Current runtime identity and state.", "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         { return ResourceContent(uri, RuntimePayload(context)); }});
    if (!added)
        return added;
    added = resources.RegisterResource(
        {"engine://runtime/snapshot", "runtime-snapshot", "Published script snapshot",
         "Last explicitly published bounded scalar snapshot; optional runtimeId and field filters.",
         "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         {
             auto query = Query(uri);
             if (!query)
                 return Invalid(query.GetError().message);
             const auto status = context.status();
             std::optional<std::string> field;
             for (const auto& [key, value] : query.Value())
             {
                 if (key == "runtimeId")
                 {
                     auto id = UUID::Parse(value);
                     if (!id || status.state == RuntimeState::Stopped ||
                         id.Value() != status.runtimeId)
                         return Invalid("Requested runtimeId is not active.");
                 }
                 else if (key == "field")
                     field = value;
                 else
                     return Invalid("Unknown snapshot query parameter.");
             }
             const auto snapshot = status.state != RuntimeState::Stopped && context.snapshot
                                       ? context.snapshot()
                                       : std::nullopt;
             Json payload = {{"available", snapshot.has_value()},
                             {"runtime", RuntimePayload(context)}};
             if (!snapshot)
             {
                 if (field)
                     return Invalid("No published snapshot field is available.");
                 payload["reason"] = status.state == RuntimeState::Stopped
                                         ? "No active runtime"
                                         : "No snapshot published";
                 return ResourceContent(uri, payload);
             }
             if (field && !snapshot->fields.contains(*field))
                 return Invalid("Unknown published snapshot field.");
             Json values = Json::object();
             for (const auto& [key, value] : snapshot->fields)
                 if (!field || key == *field)
                     std::visit([&](const auto& scalar) { values[key] = scalar; }, value);
             payload["runtimeId"] = snapshot->runtimeId.ToString();
             payload["publishedFrameIndex"] = snapshot->frameIndex;
             payload["fields"] = std::move(values);
             return ResourceContent(uri, payload);
         },
         true});
    if (!added)
        return added;
    added = resources.RegisterTemplate(
        {"engine://runtime/entity/{uuid}", "runtime-entity", "Runtime entity",
         "Read a live runtime entity by persistent UUID.", "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         {
             const auto* scene = context.scene();
             if (!scene)
                 return Invalid("No active runtime.");
             auto model = ReadEntityModel(
                 {scene, context.reflection, context.assets, {}},
                 "engine://entity/" +
                     std::string(uri.substr(std::string_view("engine://runtime/entity/").size())));
             if (!model)
                 return Invalid(model.GetError().message);
             model.Value()["runtime"] = RuntimePayload(context);
             return ResourceContent(uri, model.Value());
         }});
    if (!added)
        return added;
    added = resources.RegisterResource(
        {"engine://profiler/latest-frame", "profiler-frame", "CPU profile",
         "Last completed CPU frame. Pass counts are captured before renderer resets; absent passes "
         "are unavailable.",
         "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         {
             auto query = Query(uri);
             if (!query)
                 return Invalid(query.GetError().message);
             std::optional<u64> frameId;
             for (const auto& [key, value] : query.Value())
             {
                 u64 id = 0;
                 if (key != "frameId" || !Number(value, id))
                     return Invalid("Expected frameId unsigned decimal.");
                 frameId = id;
             }
             const auto frame = context.profile ? context.profile(frameId) : std::nullopt;
             if (!frame)
                 return ResourceContent(uri, {{"available", false},
                                              {"reason", frameId ? "Frame unavailable or evicted"
                                                                 : "No completed frame"}});
             Json scopes = Json::array();
             usize scopeBytes = 0;
             for (const auto& scope : frame->cpu.scopes)
             {
                 scopeBytes += scope.name.size() * 12 + 512;
                 if (scopeBytes > 768 * 1024)
                     break;
                 scopes.push_back({{"name", scope.name},
                                   {"parent", scope.parent},
                                   {"startMilliseconds", scope.startMilliseconds},
                                   {"durationMilliseconds", scope.durationMilliseconds}});
             }
             auto pass = [](const std::optional<RenderPassSnapshot>& value) -> Json
             {
                 if (!value)
                     return nullptr;
                 const auto& s = value->statistics;
                 return {{"entityCount", value->entityCount},
                         {"spriteCount", s.spriteCount},
                         {"drawCallCount", s.drawCallCount},
                         {"batchCount", s.batchCount},
                         {"textureBindCount", s.textureBindCount},
                         {"vertexCount", s.vertexCount},
                         {"indexCount", s.indexCount}};
             };
             return ResourceContent(uri,
                                    {{"available", true},
                                     {"frameId", frame->cpu.frameId},
                                     {"cpuMilliseconds", frame->cpu.durationMilliseconds},
                                     {"scopes", scopes},
                                     {"droppedScopes", frame->cpu.droppedScopes},
                                     {"omittedScopes", frame->cpu.scopes.size() - scopes.size()},
                                     {"runtimeId", frame->runtimeId.ToString()},
                                     {"simulationFrame", frame->simulationFrame},
                                     {"sceneView", pass(frame->sceneView)},
                                     {"gameView", pass(frame->gameView)}});
         },
         true});
    if (!added)
        return added;
    return resources.RegisterResource(
        {"engine://logs/recent", "logs-recent", "Recent logs",
         "Bounded logs: after, limit, level, category, runtimeId.", "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         {
             auto params = Query(uri);
             if (!params)
                 return Invalid(params.GetError().message);
             LogQuery query;
             for (const auto& [key, value] : params.Value())
             {
                 u64 number = 0;
                 if (key == "after" || key == "limit")
                 {
                     if (!Number(value, number))
                         return Invalid("Expected unsigned decimal query parameter.");
                     if (key == "after")
                         query.after = number;
                     else
                         query.limit = static_cast<usize>(number);
                 }
                 else if (key == "level")
                 {
                     if (value == "Info")
                         query.level = LogLevel::Info;
                     else if (value == "Warning")
                         query.level = LogLevel::Warning;
                     else if (value == "Error")
                         query.level = LogLevel::Error;
                     else
                         return Invalid("Unknown log level.");
                 }
                 else if (key == "category")
                     query.category = value;
                 else if (key == "runtimeId")
                 {
                     auto id = UUID::Parse(value);
                     if (!id)
                         return Invalid("Invalid runtimeId.");
                     query.runtimeId = id.Value();
                 }
                 else
                     return Invalid("Unknown log query parameter.");
             }
             auto page = context.logs->Read(query);
             if (!page)
                 return Invalid(page.GetError().message);
             Json entries = Json::array();
             for (const auto& entry : page.Value().entries)
             {
                 Json row = {{"sequence", entry.sequence},
                             {"timestampMilliseconds", entry.timestampMilliseconds},
                             {"level", LogLevelName(entry.level)},
                             {"category", entry.category},
                             {"message", entry.message},
                             {"truncated", entry.truncated},
                             {"runtimeId", entry.context.runtimeId.ToString()}};
                 if (entry.context.frameIndex)
                     row["frameIndex"] = *entry.context.frameIndex;
                 if (entry.context.errorCode)
                     row["errorCode"] = static_cast<i32>(*entry.context.errorCode);
                 entries.push_back(std::move(row));
             }
             return ResourceContent(uri, {{"entries", entries},
                                          {"nextCursor", page.Value().nextCursor},
                                          {"oldestSequence", page.Value().oldestSequence},
                                          {"gap", page.Value().gap},
                                          {"droppedCount", page.Value().droppedCount}});
         },
         true});
}
} // namespace Janus::MCP
