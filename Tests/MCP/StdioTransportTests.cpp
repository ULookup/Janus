#include "Protocol/McpProtocol.h"
#include "Transport/StdioTransport.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace
{

Janus::MCP::Json ModernDiscoverRequest(int id)
{
    using namespace Janus::MCP;

    return Json{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "server/discover"},
        {"params",
         Json{
             {"_meta",
              Json{
                  {std::string{McpProtocolVersionMetaKey},
                   std::string{McpModernProtocolVersion}}}}}}};
}

std::vector<Janus::MCP::Json> ParseOutputLines(
    const std::string& output)
{
    std::vector<Janus::MCP::Json> messages;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        if (line.empty())
        {
            continue;
        }

        messages.push_back(
            Janus::MCP::Json::parse(line));
    }

    return messages;
}

} // namespace

TEST_CASE(
    "stdio transport reads LF CRLF and final EOF-framed messages",
    "[mcp][stdio][v0.8]")
{
    using namespace Janus::MCP;

    std::istringstream input(
        "first\nsecond\r\nthird");
    std::ostringstream output;
    StdioTransport transport(
        input,
        output,
        64);

    auto first = transport.ReadMessage();
    REQUIRE(first);
    REQUIRE(first.Value() == std::optional<std::string>{"first"});

    auto second = transport.ReadMessage();
    REQUIRE(second);
    REQUIRE(second.Value() == std::optional<std::string>{"second"});

    auto third = transport.ReadMessage();
    REQUIRE(third);
    REQUIRE(third.Value() == std::optional<std::string>{"third"});

    auto eof = transport.ReadMessage();
    REQUIRE(eof);
    REQUIRE_FALSE(eof.Value().has_value());
}

TEST_CASE(
    "stdio transport drains oversized input before reading the next frame",
    "[mcp][stdio][v0.8]")
{
    using namespace Janus::MCP;

    std::istringstream input(
        "0123456789\nok\n");
    std::ostringstream output;
    StdioTransport transport(
        input,
        output,
        4);

    const auto oversized =
        transport.ReadMessage();
    REQUIRE_FALSE(oversized);
    REQUIRE(
        oversized.GetError().code
        == Janus::ErrorCode::InvalidArgument);

    const auto next =
        transport.ReadMessage();
    REQUIRE(next);
    REQUIRE(
        next.Value()
        == std::optional<std::string>{"ok"});
}

TEST_CASE(
    "stdio transport writes one compact JSON message per line",
    "[mcp][stdio][v0.8]")
{
    using namespace Janus::MCP;

    std::istringstream input;
    std::ostringstream output;
    StdioTransport transport(
        input,
        output,
        1024);

    REQUIRE(
        transport.WriteMessage(
            Json{
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"result",
                 Json{
                     {"text", "line one\nline two"}}}}));

    const std::string bytes =
        output.str();

    REQUIRE_FALSE(bytes.empty());
    REQUIRE(bytes.back() == '\n');
    REQUIRE(
        bytes.find('\n')
        == bytes.size() - 1);
    REQUIRE(
        bytes.find("\\n")
        != std::string::npos);

    const Json parsed =
        Json::parse(
            bytes.substr(
                0,
                bytes.size() - 1));
    REQUIRE(
        parsed.at("result").at("text")
        == "line one\nline two");
}

TEST_CASE(
    "stdio protocol serving emits protocol frames only and exits on EOF",
    "[mcp][stdio][v0.8]")
{
    using namespace Janus::MCP;

    const std::string inputBytes =
        std::string{"{\n"}
        + ModernDiscoverRequest(2).dump()
        + "\n";

    std::istringstream input(
        inputBytes);
    std::ostringstream output;

    McpProtocolSession session;
    StdioTransport transport(
        input,
        output,
        1024 * 1024);

    REQUIRE(
        ServeStdioProtocol(
            transport,
            session));

    const auto messages =
        ParseOutputLines(
            output.str());

    REQUIRE(messages.size() == 2);

    REQUIRE(
        messages[0]
            .at("error")
            .at("code")
        == JsonRpcParseError);
    REQUIRE(
        messages[1]
            .at("result")
            .at("supportedVersions")
            .at(0)
        == McpModernProtocolVersion);

    for (const Json& message : messages)
    {
        REQUIRE(
            message.at("jsonrpc")
            == "2.0");
    }
}

TEST_CASE(
    "stdio protocol converts oversized input into a controlled error and continues",
    "[mcp][stdio][v0.8]")
{
    using namespace Janus::MCP;

    const std::string valid =
        ModernDiscoverRequest(9).dump();

    std::istringstream input(
        std::string(64, 'x')
        + "\n"
        + valid
        + "\n");
    std::ostringstream output;

    McpServerConfig config;
    config.limits.maxMessageBytes = 48;

    McpProtocolSession session(
        config);
    StdioTransport transport(
        input,
        output,
        config.limits.maxMessageBytes);

    REQUIRE(
        ServeStdioProtocol(
            transport,
            session));

    const auto messages =
        ParseOutputLines(
            output.str());

    REQUIRE(messages.size() == 2);
    REQUIRE(
        messages[0]
            .at("error")
            .at("code")
        == JsonRpcInvalidRequest);
    REQUIRE(
        messages[1]
            .at("id")
        == 9);
}

TEST_CASE(
    "stdio transport rejects responses larger than its configured frame size",
    "[mcp][stdio][v0.8]")
{
    using namespace Janus::MCP;

    std::istringstream input;
    std::ostringstream output;
    StdioTransport transport(
        input,
        output,
        8);

    const auto written =
        transport.WriteMessage(
            Json{
                {"long", "response"}});

    REQUIRE_FALSE(written);
    REQUIRE(
        written.GetError().code
        == Janus::ErrorCode::InvalidArgument);
    REQUIRE(output.str().empty());
}
