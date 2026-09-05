#pragma once

#include "Core/Types.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace Janus::MCP
{

using Json = nlohmann::json;
using JsonRpcId = std::variant<i64, std::string>;

inline constexpr i32 JsonRpcParseError = -32700;
inline constexpr i32 JsonRpcInvalidRequest = -32600;
inline constexpr i32 JsonRpcMethodNotFound = -32601;
inline constexpr i32 JsonRpcInvalidParams = -32602;
inline constexpr i32 JsonRpcInternalError = -32603;

struct JsonRpcLimits
{
    usize maxMessageBytes = 1024 * 1024;
    usize maxDepth = 64;
    usize maxStringBytes = 256 * 1024;
    usize maxCollectionEntries = 4096;
};

struct JsonRpcRequest
{
    JsonRpcId id;
    std::string method;
    Json params = Json::object();
};

struct JsonRpcNotification
{
    std::string method;
    Json params = Json::object();
};

struct JsonRpcResponse
{
    JsonRpcId id;
    Json result;
};

struct JsonRpcError
{
    i32 code = JsonRpcInternalError;
    std::string message;
    Json data = nullptr;
};

struct JsonRpcErrorResponse
{
    std::optional<JsonRpcId> id;
    JsonRpcError error;
};

using JsonRpcMessage = std::variant<
    JsonRpcRequest,
    JsonRpcNotification,
    JsonRpcResponse,
    JsonRpcErrorResponse>;

struct JsonRpcDecodeError
{
    i32 code = JsonRpcInvalidRequest;
    std::string message;
};

using JsonRpcDecodeResult =
    std::variant<JsonRpcMessage, JsonRpcDecodeError>;

[[nodiscard]] JsonRpcDecodeResult DecodeJsonRpcMessage(
    std::string_view input,
    const JsonRpcLimits& limits = {});

[[nodiscard]] Json EncodeJsonRpcMessage(
    const JsonRpcMessage& message);

[[nodiscard]] Json MakeJsonRpcSuccessResponse(
    const JsonRpcId& id,
    Json result);

[[nodiscard]] Json MakeJsonRpcErrorResponse(
    std::optional<JsonRpcId> id,
    i32 code,
    std::string message,
    Json data = nullptr);

} // namespace Janus::MCP
