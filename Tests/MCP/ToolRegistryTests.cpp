#include "Registry/McpCapabilityRouter.h"
#include "Registry/ToolRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace
{

Janus::MCP::McpToolDescriptor MakeTool(
    std::string name)
{
    using namespace Janus::MCP;

    return McpToolDescriptor{
        std::move(name),
        {},
        "test tool",
        Json{
            {"type", "object"},
            {"properties", Json::object()},
            {"additionalProperties", false}},
        std::nullopt,
        Json::object(),
        [](const Json& arguments,
           McpProtocolEra) -> McpDispatchResult
        {
            return Json{
                {"content", Json::array()},
                {"structuredContent", arguments}};
        }};
}

const Janus::MCP::Json& RequireJson(
    const Janus::MCP::McpDispatchResult& result)
{
    REQUIRE(std::holds_alternative<Janus::MCP::Json>(result));
    return std::get<Janus::MCP::Json>(result);
}

const Janus::MCP::McpDispatchError& RequireError(
    const Janus::MCP::McpDispatchResult& result)
{
    REQUIRE(std::holds_alternative<Janus::MCP::McpDispatchError>(result));
    return std::get<Janus::MCP::McpDispatchError>(result);
}

} // namespace

TEST_CASE(
    "ToolRegistry registers tools deterministically and normalizes schemas",
    "[mcp][registry][tool][v0.8]")
{
    using namespace Janus::MCP;

    ToolRegistry registry;

    REQUIRE(registry.RegisterTool(MakeTool("z.tool")));
    REQUIRE(registry.RegisterTool(MakeTool("a.tool")));
    REQUIRE(registry.GetToolCount() == 2);

    const auto tools =
        registry.GetTools();

    REQUIRE(tools.size() == 2);
    REQUIRE(tools[0]->name == "a.tool");
    REQUIRE(tools[1]->name == "z.tool");

    REQUIRE(
        tools[0]->inputSchema.at("$schema")
        == std::string{McpJsonSchema202012});
    REQUIRE(
        tools[0]->inputSchema.at("type")
        == "object");

    const auto duplicate =
        registry.RegisterTool(
            MakeTool("a.tool"));

    REQUIRE_FALSE(duplicate);
    REQUIRE(
        duplicate.GetError().code
        == Janus::ErrorCode::InvalidArgument);
}

TEST_CASE(
    "ToolRegistry rejects invalid descriptors",
    "[mcp][registry][tool][v0.8]")
{
    using namespace Janus::MCP;

    ToolRegistry registry;

    auto missingHandler =
        MakeTool("missing");
    missingHandler.handler = {};

    REQUIRE_FALSE(
        registry.RegisterTool(
            std::move(missingHandler)));

    auto wrongRoot =
        MakeTool("wrong-root");
    wrongRoot.inputSchema = {
        {"type", "array"}};

    REQUIRE_FALSE(
        registry.RegisterTool(
            std::move(wrongRoot)));

    auto wrongDialect =
        MakeTool("wrong-dialect");
    wrongDialect.inputSchema["$schema"] =
        "http://json-schema.org/draft-07/schema#";

    REQUIRE_FALSE(
        registry.RegisterTool(
            std::move(wrongDialect)));
}

TEST_CASE(
    "ToolRegistry list is paged deterministic and era-aware",
    "[mcp][registry][tool][v0.8]")
{
    using namespace Janus::MCP;

    ToolRegistry registry;

    for (int index = 0;
         index < 65;
         ++index)
    {
        std::string name =
            "tool."
            + std::to_string(100 + index);

        REQUIRE(
            registry.RegisterTool(
                MakeTool(
                    std::move(name))));
    }

    const Json& first =
        RequireJson(
            registry.HandleList(
                Json::object(),
                McpProtocolEra::Modern2026));

    REQUIRE(first.at("tools").size() == 64);
    REQUIRE(first.at("nextCursor") == "64");
    REQUIRE(first.at("ttlMs") == 0);
    REQUIRE(first.at("cacheScope") == "private");

    const Json& second =
        RequireJson(
            registry.HandleList(
                Json{{"cursor", "64"}},
                McpProtocolEra::Modern2026));

    REQUIRE(second.at("tools").size() == 1);
    REQUIRE_FALSE(second.contains("nextCursor"));

    const Json& legacy =
        RequireJson(
            registry.HandleList(
                Json::object(),
                McpProtocolEra::Legacy2025));

    REQUIRE_FALSE(legacy.contains("ttlMs"));
    REQUIRE_FALSE(legacy.contains("cacheScope"));

    const auto invalid =
        registry.HandleList(
            Json{{"cursor", "not-a-number"}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireError(invalid).code
        == JsonRpcInvalidParams);
}

TEST_CASE(
    "ToolRegistry dispatches tools by stable name",
    "[mcp][registry][tool][v0.8]")
{
    using namespace Janus::MCP;

    ToolRegistry registry;
    bool called = false;

    auto descriptor =
        MakeTool("echo");
    descriptor.handler =
        [&called](
            const Json& arguments,
            McpProtocolEra era) -> McpDispatchResult
        {
            called = true;
            REQUIRE(
                era
                == McpProtocolEra::Modern2026);
            REQUIRE(
                arguments.at("value")
                == 3);

            return Json{
                {"content", Json::array()},
                {"structuredContent",
                 Json{{"value", 3}}}};
        };

    REQUIRE(
        registry.RegisterTool(
            std::move(descriptor)));

    const Json& result =
        RequireJson(
            registry.HandleCall(
                Json{
                    {"name", "echo"},
                    {"arguments",
                     Json{{"value", 3}}}},
                McpProtocolEra::Modern2026));

    REQUIRE(called);
    REQUIRE(
        result.at("structuredContent")
            .at("value")
        == 3);

    const auto missing =
        registry.HandleCall(
            Json{{"name", "missing"}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireError(missing).code
        == JsonRpcInvalidParams);
}

TEST_CASE(
    "McpCapabilityRouter forwards registry methods and rejects unknown methods",
    "[mcp][registry][router][v0.8]")
{
    using namespace Janus::MCP;

    ToolRegistry tools;
    ResourceRegistry resources;
    McpCapabilityRouter router(
        tools,
        resources);

    REQUIRE(
        tools.RegisterTool(
            MakeTool("echo")));

    const auto listed =
        router.HandleRequest(
            "tools/list",
            Json::object(),
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireJson(listed)
            .at("tools")
            .size()
        == 1);

    const auto missing =
        router.HandleRequest(
            "unknown/method",
            Json::object(),
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireError(missing).code
        == JsonRpcMethodNotFound);
}
