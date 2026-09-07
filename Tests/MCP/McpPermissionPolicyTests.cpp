#include "Host/McpPermissionPolicy.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "MCP permission classifier distinguishes read write and save operations",
    "[mcp][host][permission][v0.8]")
{
    using namespace Janus::MCP;

    REQUIRE(ClassifyMcpOperation("tools/call", Json{{"name", "assets.search"}}) ==
            McpOperation::ProjectRead);

    REQUIRE(ClassifyMcpOperation("tools/call", Json{{"name", "scene.reparent_entity"}}) ==
            McpOperation::SceneWrite);

    REQUIRE(
        ClassifyMcpOperation(
            "resources/list",
            Json::object())
        == McpOperation::ProjectRead);

    REQUIRE(
        ClassifyMcpOperation(
            "resources/read",
            Json{
                {"uri",
                 "engine://project/info"}})
        == McpOperation::ProjectRead);

    REQUIRE(
        ClassifyMcpOperation(
            "resources/read",
            Json{
                {"uri",
                 "engine://scene/current"}})
        == McpOperation::SceneRead);

    REQUIRE(
        ClassifyMcpOperation(
            "resources/read",
            Json{
                {"uri",
                 "engine://entity/abc"}})
        == McpOperation::SceneRead);

    REQUIRE(
        ClassifyMcpOperation(
            "tools/call",
            Json{
                {"name",
                 "scene.create_entity"}})
        == McpOperation::SceneWrite);

    REQUIRE(
        ClassifyMcpOperation(
            "tools/call",
            Json{
                {"name",
                 "scene.save"}})
        == McpOperation::SceneSave);
}

TEST_CASE(
    "MCP request target preserves tool and resource identity",
    "[mcp][host][permission][v0.8]")
{
    using namespace Janus::MCP;

    REQUIRE(
        McpRequestTarget(
            "tools/call",
            Json{
                {"name",
                 "scene.rename_entity"}})
        == "scene.rename_entity");

    REQUIRE(
        McpRequestTarget(
            "resources/read",
            Json{
                {"uri",
                 "engine://scene/hierarchy"}})
        == "engine://scene/hierarchy");

    REQUIRE(
        McpRequestTarget(
            "tools/list",
            Json::object())
        == "tools/list");
}

TEST_CASE(
    "Default local MCP permission policy allows classified operations",
    "[mcp][host][permission][v0.8]")
{
    Janus::MCP::AllowAllMcpPermissionPolicy policy;

    const auto result =
        policy.Authorize(
            Janus::MCP::McpOperation::SceneWrite,
            Janus::MCP::McpRequestContext{
                "tools/call",
                "scene.create_entity",
                Janus::MCP::McpProtocolEra::Modern2026});

    REQUIRE(result);
}

TEST_CASE("Unknown MCP operations never downgrade to project read", "[permission][v0.9]")
{
    using namespace Janus::MCP;
    REQUIRE(ClassifyMcpOperation("tools/call", {{"name", "runtime.destroy_everything"}}) ==
            McpOperation::Unclassified);
    REQUIRE(ClassifyMcpOperation("tools/call", {{"name", "scene.unknown"}}) ==
            McpOperation::Unclassified);
    REQUIRE(ClassifyMcpOperation("tools/call", {{"name", "runtime.play"}}) ==
            McpOperation::RuntimeControl);
    REQUIRE(ClassifyMcpOperation("resources/read", {{"uri", "engine://logs/recent?limit=2"}}) ==
            McpOperation::DiagnosticsRead);
}

TEST_CASE("Published snapshots are explicitly RuntimeRead and similar names stay denied",
          "[snapshot][permission]")
{
    using namespace Janus::MCP;
    REQUIRE(
        ClassifyMcpOperation("resources/read", {{"uri", "engine://runtime/snapshot?field=hp"}}) ==
        McpOperation::RuntimeRead);
    REQUIRE(ClassifyMcpOperation("resources/read", {{"uri", "engine://runtime/snapshot/eval"}}) ==
            McpOperation::Unclassified);
}
