#include "Protocol/JsonRpc.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <variant>

namespace
{

const Janus::MCP::JsonRpcMessage& RequireMessage(
    const Janus::MCP::JsonRpcDecodeResult& decoded)
{
    REQUIRE(std::holds_alternative<Janus::MCP::JsonRpcMessage>(decoded));
    return std::get<Janus::MCP::JsonRpcMessage>(decoded);
}

const Janus::MCP::JsonRpcDecodeError& RequireDecodeError(
    const Janus::MCP::JsonRpcDecodeResult& decoded)
{
    REQUIRE(std::holds_alternative<Janus::MCP::JsonRpcDecodeError>(decoded));
    return std::get<Janus::MCP::JsonRpcDecodeError>(decoded);
}

} // namespace

TEST_CASE(
    "JSON-RPC decodes requests notifications and responses",
    "[mcp][json-rpc][v0.8]")
{
    using namespace Janus::MCP;

    SECTION("integer request id")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":7,"method":"tools/list","params":{"cursor":"a"}})json");

        const auto& message = RequireMessage(decoded);
        const auto* request = std::get_if<JsonRpcRequest>(&message);
        REQUIRE(request != nullptr);
        REQUIRE(std::get<Janus::i64>(request->id) == 7);
        REQUIRE(request->method == "tools/list");
        REQUIRE(request->params.at("cursor") == "a");
    }

    SECTION("string request id")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":"req-1","method":"resources/list"})json");

        const auto& message = RequireMessage(decoded);
        const auto* request = std::get_if<JsonRpcRequest>(&message);
        REQUIRE(request != nullptr);
        REQUIRE(std::get<std::string>(request->id) == "req-1");
        REQUIRE(request->params == Json::object());
    }

    SECTION("notification")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","method":"notifications/initialized"})json");

        const auto& message = RequireMessage(decoded);
        const auto* notification = std::get_if<JsonRpcNotification>(&message);
        REQUIRE(notification != nullptr);
        REQUIRE(notification->method == "notifications/initialized");
        REQUIRE(notification->params == Json::object());
    }

    SECTION("success response")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":2,"result":{"ok":true}})json");

        const auto& message = RequireMessage(decoded);
        const auto* response = std::get_if<JsonRpcResponse>(&message);
        REQUIRE(response != nullptr);
        REQUIRE(response->result.at("ok") == true);
    }

    SECTION("error response with null id")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"bad"}})json");

        const auto& message = RequireMessage(decoded);
        const auto* response = std::get_if<JsonRpcErrorResponse>(&message);
        REQUIRE(response != nullptr);
        REQUIRE_FALSE(response->id.has_value());
        REQUIRE(response->error.code == JsonRpcParseError);
        REQUIRE(response->error.message == "bad");
    }
}

TEST_CASE(
    "JSON-RPC rejects malformed or ambiguous messages",
    "[mcp][json-rpc][v0.8]")
{
    using namespace Janus::MCP;

    SECTION("malformed JSON")
    {
        const auto decoded = DecodeJsonRpcMessage("{");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcParseError);
    }

    SECTION("wrong protocol version")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"1.0","id":1,"method":"tools/list"})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("method must be a string")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":1,"method":42})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("params must be object or array")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":1,"method":"x","params":"bad"})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("request id must be integer or string")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":null,"method":"x"})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("response cannot contain result and error")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":1,"result":{},"error":{"code":-1,"message":"bad"}})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("unsigned request id cannot overflow signed storage")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":18446744073709551615,"method":"x"})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("error code must fit 32 bits")
    {
        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":null,"error":{"code":9223372036854775807,"message":"bad"}})json");
        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }
}

TEST_CASE(
    "JSON-RPC enforces configured input bounds",
    "[mcp][json-rpc][v0.8]")
{
    using namespace Janus::MCP;

    SECTION("message bytes")
    {
        JsonRpcLimits limits;
        limits.maxMessageBytes = 20;

        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","method":"x"})json",
            limits);

        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("string bytes")
    {
        JsonRpcLimits limits;
        limits.maxStringBytes = 3;

        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","method":"long"})json",
            limits);

        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("collection entries")
    {
        JsonRpcLimits limits;
        limits.maxCollectionEntries = 2;

        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":1,"method":"x"})json",
            limits);

        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }

    SECTION("nesting depth")
    {
        JsonRpcLimits limits;
        limits.maxDepth = 2;

        const auto decoded = DecodeJsonRpcMessage(
            R"json({"jsonrpc":"2.0","id":1,"method":"x","params":{"a":{"b":{"c":1}}}})json",
            limits);

        const auto& error = RequireDecodeError(decoded);
        REQUIRE(error.code == JsonRpcInvalidRequest);
    }
}

TEST_CASE(
    "JSON-RPC response helpers preserve ids and structured data",
    "[mcp][json-rpc][v0.8]")
{
    using namespace Janus::MCP;

    const Json success = MakeJsonRpcSuccessResponse(
        JsonRpcId{std::string{"abc"}},
        Json{{"count", 3}});

    REQUIRE(success.at("jsonrpc") == "2.0");
    REQUIRE(success.at("id") == "abc");
    REQUIRE(success.at("result").at("count") == 3);

    const Json failure = MakeJsonRpcErrorResponse(
        JsonRpcId{Janus::i64{9}},
        JsonRpcInvalidParams,
        "invalid",
        Json{{"field", "entity"}});

    REQUIRE(failure.at("id") == 9);
    REQUIRE(failure.at("error").at("code") == JsonRpcInvalidParams);
    REQUIRE(failure.at("error").at("data").at("field") == "entity");
}
