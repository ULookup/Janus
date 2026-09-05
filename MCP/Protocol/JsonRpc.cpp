#include "Protocol/JsonRpc.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace Janus::MCP
{
namespace
{

std::optional<JsonRpcId> ParseId(const Json& value)
{
    if (value.is_string())
    {
        return JsonRpcId{value.get<std::string>()};
    }

    if (value.is_number_unsigned())
    {
        const u64 raw = value.get<u64>();
        if (raw <= static_cast<u64>(std::numeric_limits<i64>::max()))
        {
            return JsonRpcId{static_cast<i64>(raw)};
        }

        return std::nullopt;
    }

    if (value.is_number_integer())
    {
        return JsonRpcId{value.get<i64>()};
    }

    return std::nullopt;
}

Json EncodeId(const JsonRpcId& id)
{
    return std::visit(
        [](const auto& value) -> Json
        {
            return Json{value};
        },
        id);
}

bool ValidateBounds(
    const Json& value,
    const JsonRpcLimits& limits,
    usize depth)
{
    if (depth > limits.maxDepth)
    {
        return false;
    }

    if (value.is_string()
        && value.get_ref<const std::string&>().size()
            > limits.maxStringBytes)
    {
        return false;
    }

    if (value.is_array())
    {
        if (value.size() > limits.maxCollectionEntries)
        {
            return false;
        }

        for (const Json& element : value)
        {
            if (!ValidateBounds(element, limits, depth + 1))
            {
                return false;
            }
        }
    }
    else if (value.is_object())
    {
        if (value.size() > limits.maxCollectionEntries)
        {
            return false;
        }

        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (it.key().size() > limits.maxStringBytes
                || !ValidateBounds(it.value(), limits, depth + 1))
            {
                return false;
            }
        }
    }

    return true;
}

JsonRpcDecodeResult InvalidRequest(std::string message)
{
    return JsonRpcDecodeError{
        JsonRpcInvalidRequest,
        std::move(message)};
}

JsonRpcDecodeResult DecodeRequest(const Json& root)
{
    const auto methodIt = root.find("method");
    if (methodIt == root.end() || !methodIt->is_string())
    {
        return InvalidRequest(
            "JSON-RPC request method must be a string.");
    }

    if (root.contains("result") || root.contains("error"))
    {
        return InvalidRequest(
            "JSON-RPC request cannot contain result or error.");
    }

    Json params = Json::object();
    if (const auto paramsIt = root.find("params");
        paramsIt != root.end())
    {
        if (!paramsIt->is_object() && !paramsIt->is_array())
        {
            return InvalidRequest(
                "JSON-RPC params must be an object or array.");
        }

        params = *paramsIt;
    }

    const auto idIt = root.find("id");
    if (idIt == root.end())
    {
        return JsonRpcMessage{
            JsonRpcNotification{
                methodIt->get<std::string>(),
                std::move(params)}};
    }

    const auto id = ParseId(*idIt);
    if (!id.has_value())
    {
        return InvalidRequest(
            "JSON-RPC request id must be an integer or string.");
    }

    return JsonRpcMessage{
        JsonRpcRequest{
            *id,
            methodIt->get<std::string>(),
            std::move(params)}};
}

JsonRpcDecodeResult DecodeResponse(const Json& root)
{
    const auto idIt = root.find("id");
    if (idIt == root.end())
    {
        return InvalidRequest(
            "JSON-RPC response requires an id.");
    }

    const bool hasResult = root.contains("result");
    const bool hasError = root.contains("error");

    if (hasResult == hasError)
    {
        return InvalidRequest(
            "JSON-RPC response requires exactly one of result or error.");
    }

    if (hasResult)
    {
        const auto id = ParseId(*idIt);
        if (!id.has_value())
        {
            return InvalidRequest(
                "JSON-RPC success response id must be an integer or string.");
        }

        return JsonRpcMessage{
            JsonRpcResponse{
                *id,
                root.at("result")}};
    }

    std::optional<JsonRpcId> id;
    if (!idIt->is_null())
    {
        id = ParseId(*idIt);
        if (!id.has_value())
        {
            return InvalidRequest(
                "JSON-RPC error response id must be null, integer, or string.");
        }
    }

    const Json& error = root.at("error");
    if (!error.is_object())
    {
        return InvalidRequest(
            "JSON-RPC error must be an object.");
    }

    const auto codeIt = error.find("code");
    const auto messageIt = error.find("message");

    if (codeIt == error.end()
        || !codeIt->is_number_integer()
        || messageIt == error.end()
        || !messageIt->is_string())
    {
        return InvalidRequest(
            "JSON-RPC error requires integer code and string message.");
    }

    const i64 rawCode = codeIt->get<i64>();
    if (rawCode < static_cast<i64>(std::numeric_limits<i32>::min())
        || rawCode > static_cast<i64>(std::numeric_limits<i32>::max()))
    {
        return InvalidRequest(
            "JSON-RPC error code is outside the supported 32-bit range.");
    }

    Json data = nullptr;
    if (const auto dataIt = error.find("data");
        dataIt != error.end())
    {
        data = *dataIt;
    }

    return JsonRpcMessage{
        JsonRpcErrorResponse{
            id,
            JsonRpcError{
                static_cast<i32>(rawCode),
                messageIt->get<std::string>(),
                std::move(data)}}};
}

} // namespace

JsonRpcDecodeResult DecodeJsonRpcMessage(
    std::string_view input,
    const JsonRpcLimits& limits)
{
    if (input.size() > limits.maxMessageBytes)
    {
        return InvalidRequest(
            "JSON-RPC message exceeds the configured size limit.");
    }

    bool depthExceeded = false;

    const auto callback =
        [&depthExceeded, &limits](
            int depth,
            Json::parse_event_t,
            Json&) -> bool
    {
        if (depth < 0
            || static_cast<usize>(depth) > limits.maxDepth)
        {
            depthExceeded = true;
            return false;
        }

        return true;
    };

    Json root = Json::parse(
        input.begin(),
        input.end(),
        callback,
        false);

    if (depthExceeded)
    {
        return InvalidRequest(
            "JSON-RPC message exceeds the configured nesting limit.");
    }

    if (root.is_discarded())
    {
        return JsonRpcDecodeError{
            JsonRpcParseError,
            "Invalid JSON."};
    }

    if (!ValidateBounds(root, limits, 0))
    {
        return InvalidRequest(
            "JSON-RPC message exceeds configured value limits.");
    }

    if (!root.is_object())
    {
        return InvalidRequest(
            "JSON-RPC message root must be an object.");
    }

    const auto versionIt = root.find("jsonrpc");
    if (versionIt == root.end()
        || !versionIt->is_string()
        || versionIt->get_ref<const std::string&>() != "2.0")
    {
        return InvalidRequest(
            "JSON-RPC version must be exactly '2.0'.");
    }

    if (root.contains("method"))
    {
        return DecodeRequest(root);
    }

    if (root.contains("result") || root.contains("error"))
    {
        return DecodeResponse(root);
    }

    return InvalidRequest(
        "JSON-RPC message is neither a request nor a response.");
}

Json EncodeJsonRpcMessage(const JsonRpcMessage& message)
{
    return std::visit(
        [](const auto& value) -> Json
        {
            using T = std::decay_t<decltype(value)>;

            Json root = {
                {"jsonrpc", "2.0"}};

            if constexpr (std::is_same_v<T, JsonRpcRequest>)
            {
                root["id"] = EncodeId(value.id);
                root["method"] = value.method;
                if (!value.params.empty())
                {
                    root["params"] = value.params;
                }
            }
            else if constexpr (
                std::is_same_v<T, JsonRpcNotification>)
            {
                root["method"] = value.method;
                if (!value.params.empty())
                {
                    root["params"] = value.params;
                }
            }
            else if constexpr (
                std::is_same_v<T, JsonRpcResponse>)
            {
                root["id"] = EncodeId(value.id);
                root["result"] = value.result;
            }
            else
            {
                root["id"] = value.id.has_value()
                    ? EncodeId(*value.id)
                    : Json{nullptr};

                root["error"] = {
                    {"code", value.error.code},
                    {"message", value.error.message}};

                if (!value.error.data.is_null())
                {
                    root["error"]["data"] =
                        value.error.data;
                }
            }

            return root;
        },
        message);
}

Json MakeJsonRpcSuccessResponse(
    const JsonRpcId& id,
    Json result)
{
    return EncodeJsonRpcMessage(
        JsonRpcMessage{
            JsonRpcResponse{
                id,
                std::move(result)}});
}

Json MakeJsonRpcErrorResponse(
    std::optional<JsonRpcId> id,
    i32 code,
    std::string message,
    Json data)
{
    return EncodeJsonRpcMessage(
        JsonRpcMessage{
            JsonRpcErrorResponse{
                std::move(id),
                JsonRpcError{
                    code,
                    std::move(message),
                    std::move(data)}}});
}

} // namespace Janus::MCP
