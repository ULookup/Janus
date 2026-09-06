#include "Tools/TransactionTools.h"
#include "Resources/DebugResources.h"
#include <charconv>
namespace Janus::MCP
{
namespace
{
McpDispatchError Invalid(std::string message)
{
    return {JsonRpcInvalidParams, std::move(message), nullptr};
}

} // namespace
Json TransactionStatusJson(const CommandBus& commands)
{
    return {{"state", commands.RecoveryRequired() ? "RecoveryRequired"
                      : commands.HasTransaction() ? "Active"
                                                  : "Idle"},
            {"transaction", commands.GetTransactionId().IsValid()
                                ? Json(commands.GetTransactionId().ToString())
                                : Json(nullptr)},
            {"provisional", commands.HasTransaction()},
            {"commandCount", commands.GetPendingCount()},
            {"reservedUndoBytes", commands.GetPendingBytes()},
            {"maxCommands", 64},
            {"maxUndoBytes", 8 * 1024 * 1024},
            {"timeoutSeconds", 60}};
}
Result<void> RegisterTransactionCapabilities(ToolRegistry& tools, ResourceRegistry& resources,
                                             McpTransactionContext context)
{
    if (!context.commands || !context.begin || !context.finish)
        return Result<void>::Failure(ErrorCode::InvalidArgument, "Transaction callbacks required.");
    for (std::string name : {"transaction.begin", "transaction.commit", "transaction.rollback"})
    {
        const bool begin = name == "transaction.begin";
        McpToolDescriptor tool;
        tool.name = name;
        tool.title = name;
        tool.description = "Coordinate one provisional, exclusively owned authoring group. Commit "
                           "creates one undo entry.";
        tool.inputSchema = {{"$schema", McpJsonSchema202012},
                            {"type", "object"},
                            {"properties", Json::object()},
                            {"additionalProperties", false}};
        if (begin)
            tool.inputSchema["properties"]["label"] = {{"type", "string"}, {"maxLength", 128}};
        if (!begin)
        {
            tool.inputSchema["properties"]["transaction"] = {{"type", "string"},
                                                             {"format", "uuid"}};
            tool.inputSchema["required"] = Json::array({"transaction"});
        }
        tool.handler = [context, name, begin](const Json& args, McpProtocolEra) -> McpDispatchResult
        {
            if (!args.is_object() ||
                (begin ? (args.size() > 1 ||
                          (!args.empty() &&
                           (!args.contains("label") || !args["label"].is_string() ||
                            args["label"].get_ref<const std::string&>().size() > 128)))
                       : args.size() != 1 || !args.contains("transaction") ||
                             !args["transaction"].is_string()))
                return Invalid("Invalid transaction arguments.");
            Json response;
            if (begin)
            {
                auto token = context.begin(args.value("label", std::string{}));
                response = OperationResult(token ? Result<void>::Success()
                                                 : Result<void>::Failure(token.GetError()));
                if (token)
                    response["structuredContent"]["transaction"] = token.Value().ToString();
            }
            else
            {
                auto token = UUID::Parse(args["transaction"].get<std::string>());
                if (!token || !token.Value().IsValid())
                    return Invalid("Invalid transaction UUID.");
                response =
                    OperationResult(context.finish(token.Value(), name == "transaction.commit"));
            }
            response["structuredContent"]["status"] = TransactionStatusJson(*context.commands);
            response["content"][0]["text"] = response["structuredContent"].dump();
            return response;
        };
        auto added = tools.RegisterTool(std::move(tool));
        if (!added)
            return added;
    }
    auto added = resources.RegisterResource(
        {"engine://transaction/status", "transaction-status", "Authoring transaction",
         "Provisional state and recovery requirement.", "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         { return ResourceContent(uri, TransactionStatusJson(*context.commands)); }});
    if (!added)
        return added;
    return resources.RegisterResource(
        {"engine://agent/activity", "agent-activity", "Authoring activity",
         "Bounded correlated command receipts. Query after and limit (1..200).", "application/json",
         [context](std::string_view uri, McpProtocolEra) -> McpDispatchResult
         {
             const auto& rows = context.commands->GetActivity();
             u64 after = 0, limit = 100;
             bool hasAfter = false, hasLimit = false;
             std::optional<UUID> transaction;
             auto query = uri.find('?') == std::string_view::npos ? std::string_view{}
                                                                  : uri.substr(uri.find('?') + 1);
             while (!query.empty())
             {
                 auto pair = query.substr(0, query.find('&'));
                 const auto eq = pair.find('=');
                 if (eq == std::string_view::npos)
                     return Invalid("Malformed activity query.");
                 auto key = pair.substr(0, eq), value = pair.substr(eq + 1);
                 u64 parsed = 0;
                 if (key == "transactionId")
                 {
                     auto token = UUID::Parse(value);
                     if (!token || transaction)
                         return Invalid("Invalid or duplicate transactionId filter.");
                     transaction = token.Value();
                 }
                 else
                 {
                     auto result =
                         std::from_chars(value.data(), value.data() + value.size(), parsed);
                     if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                         return Invalid("Invalid activity cursor or limit.");
                     if (key == "after" && !hasAfter)
                     {
                         after = parsed;
                         hasAfter = true;
                     }
                     else if (key == "limit" && !hasLimit)
                     {
                         limit = parsed;
                         hasLimit = true;
                     }
                     else
                         return Invalid("Unknown or duplicate activity query.");
                 }
                 if (query.find('&') == std::string_view::npos)
                     break;
                 query.remove_prefix(query.find('&') + 1);
             }
             if (limit == 0 || limit > 200)
                 return Invalid("Activity limit must be 1..200.");
             const auto next = context.commands->GetNextActivitySequence();
             const auto oldest = rows.empty() ? next : rows.front().sequence;
             if (!hasAfter)
             {
                 after = next - 1;
                 u64 found = 0;
                 for (auto it = rows.rbegin(); it != rows.rend(); ++it)
                     if (!transaction || it->transaction == *transaction)
                     {
                         after = it->sequence - 1;
                         if (++found == limit)
                             break;
                     }
             }
             u64 cursor = std::min(after, next - 1);
             Json entries = Json::array();
             usize bytes = 0;
             for (const auto& row : rows)
             {
                 if (row.sequence <= after)
                     continue;
                 if (transaction && row.transaction != *transaction)
                 {
                     cursor = row.sequence;
                     continue;
                 }
                 Json effects = Json::array();
                 for (const auto& effect : row.effects)
                     effects.push_back({{"entity", effect.entity.ToString()},
                                        {"operation", effect.operation},
                                        {"component", std::to_string(effect.component)},
                                        {"property", std::to_string(effect.property)}});
                 Json entry = {{"sequence", row.sequence},
                               {"commandId", std::to_string(row.commandId)},
                               {"transaction", row.transaction.ToString()},
                               {"actor", CommandActorName(row.actor)},
                               {"outcome", CommandOutcomeName(row.outcome)},
                               {"description", row.description},
                               {"effects", effects},
                               {"effectCount", row.effectCount},
                               {"truncated", row.truncated},
                               {"timestampMilliseconds", row.timestampMilliseconds},
                               {"sessionId", row.sessionId.ToString()}};
                 if (row.error)
                     entry["error"] = {{"code", static_cast<i32>(row.error->code)},
                                       {"message", row.error->message}};
                 const auto cost =
                     entry.dump(-1, ' ', false, Json::error_handler_t::replace).size() * 2;
                 if (entries.size() == limit || bytes + cost > 768 * 1024)
                     break;
                 bytes += cost;
                 entries.push_back(std::move(entry));
                 cursor = row.sequence;
             }
             return ResourceContent(uri, {{"entries", entries},
                                          {"nextCursor", cursor},
                                          {"oldestSequence", oldest},
                                          {"gap", hasAfter && after < oldest - 1}});
         },
         true});
}
} // namespace Janus::MCP
