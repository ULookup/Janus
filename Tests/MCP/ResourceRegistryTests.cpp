#include "Registry/ResourceRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <variant>

namespace
{

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

Janus::MCP::McpResourceDescriptor MakeResource(
    std::string uri,
    std::string name)
{
    using namespace Janus::MCP;

    return McpResourceDescriptor{
        std::move(uri),
        std::move(name),
        {},
        "test resource",
        "application/json",
        [](std::string_view readUri,
           McpProtocolEra) -> McpDispatchResult
        {
            return Json{
                {"contents",
                 Json::array(
                     {Json{
                         {"uri", std::string{readUri}},
                         {"mimeType", "application/json"},
                         {"text", "{}"}}})}};
        }};
}

Janus::MCP::McpResourceTemplateDescriptor MakeTemplate(
    std::string uriTemplate,
    std::string name)
{
    using namespace Janus::MCP;

    return McpResourceTemplateDescriptor{
        std::move(uriTemplate),
        std::move(name),
        {},
        "test resource template",
        "application/json",
        [](std::string_view readUri,
           McpProtocolEra) -> McpDispatchResult
        {
            return Json{
                {"contents",
                 Json::array(
                     {Json{
                         {"uri", std::string{readUri}},
                         {"mimeType", "application/json"},
                         {"text", R"json({"ok":true})json"}}})}};
        }};
}

} // namespace

TEST_CASE(
    "ResourceRegistry registers concrete resources deterministically",
    "[mcp][registry][resource][v0.8]")
{
    using namespace Janus::MCP;

    ResourceRegistry registry;

    REQUIRE(
        registry.RegisterResource(
            MakeResource(
                "engine://z",
                "z")));
    REQUIRE(
        registry.RegisterResource(
            MakeResource(
                "engine://a",
                "a")));

    const auto resources =
        registry.GetResources();

    REQUIRE(resources.size() == 2);
    REQUIRE(resources[0]->uri == "engine://a");
    REQUIRE(resources[1]->uri == "engine://z");

    const auto duplicate =
        registry.RegisterResource(
            MakeResource(
                "engine://a",
                "duplicate"));

    REQUIRE_FALSE(duplicate);
}

TEST_CASE(
    "ResourceRegistry validates and matches resource templates",
    "[mcp][registry][resource][v0.8]")
{
    using namespace Janus::MCP;

    ResourceRegistry registry;

    REQUIRE(
        registry.RegisterTemplate(
            MakeTemplate(
                "engine://entity/{uuid}",
                "entity")));

    REQUIRE(
        registry.RegisterTemplate(
            MakeTemplate(
                "engine://asset/{uuid}",
                "asset")));

    REQUIRE(
        registry.FindTemplate(
            "engine://entity/{uuid}")
        != nullptr);

    const auto* entity =
        registry.MatchTemplate(
            "engine://entity/1234");

    REQUIRE(entity != nullptr);
    REQUIRE(entity->name == "entity");

    REQUIRE(
        registry.MatchTemplate(
            "engine://entity/")
        == nullptr);

    const auto invalid =
        registry.RegisterTemplate(
            MakeTemplate(
                "engine://entity/{}",
                "invalid"));

    REQUIRE_FALSE(invalid);

    const auto duplicate =
        registry.RegisterTemplate(
            MakeTemplate(
                "engine://entity/{uuid}",
                "duplicate"));

    REQUIRE_FALSE(duplicate);
}

TEST_CASE(
    "ResourceRegistry lists resources and templates with modern cache hints",
    "[mcp][registry][resource][v0.8]")
{
    using namespace Janus::MCP;

    ResourceRegistry registry;

    REQUIRE(
        registry.RegisterResource(
            MakeResource(
                "engine://project/info",
                "project")));
    REQUIRE(
        registry.RegisterTemplate(
            MakeTemplate(
                "engine://entity/{uuid}",
                "entity")));

    const Json& resources =
        RequireJson(
            registry.HandleList(
                Json::object(),
                McpProtocolEra::Modern2026));

    REQUIRE(
        resources.at("resources")
            .size()
        == 1);
    REQUIRE(resources.at("ttlMs") == 0);
    REQUIRE(
        resources.at("cacheScope")
        == "private");

    const Json& templates =
        RequireJson(
            registry.HandleTemplatesList(
                Json::object(),
                McpProtocolEra::Modern2026));

    REQUIRE(
        templates.at("resourceTemplates")
            .size()
        == 1);
    REQUIRE(templates.at("ttlMs") == 0);
    REQUIRE(
        templates.at("cacheScope")
        == "private");

    const Json& legacy =
        RequireJson(
            registry.HandleTemplatesList(
                Json::object(),
                McpProtocolEra::Legacy2025));

    REQUIRE_FALSE(legacy.contains("ttlMs"));
    REQUIRE_FALSE(
        legacy.contains("cacheScope"));
}

TEST_CASE(
    "ResourceRegistry reads concrete and templated resources",
    "[mcp][registry][resource][v0.8]")
{
    using namespace Janus::MCP;

    ResourceRegistry registry;

    REQUIRE(
        registry.RegisterResource(
            MakeResource(
                "engine://scene/current",
                "scene")));
    REQUIRE(
        registry.RegisterTemplate(
            MakeTemplate(
                "engine://entity/{uuid}",
                "entity")));

    const Json& concrete =
        RequireJson(
            registry.HandleRead(
                Json{
                    {"uri",
                     "engine://scene/current"}},
                McpProtocolEra::Modern2026));

    REQUIRE(
        concrete.at("contents")
            .at(0)
            .at("uri")
        == "engine://scene/current");
    REQUIRE(concrete.at("ttlMs") == 0);
    REQUIRE(
        concrete.at("cacheScope")
        == "private");

    const Json& templated =
        RequireJson(
            registry.HandleRead(
                Json{
                    {"uri",
                     "engine://entity/abc"}},
                McpProtocolEra::Legacy2025));

    REQUIRE(
        templated.at("contents")
            .at(0)
            .at("uri")
        == "engine://entity/abc");
    REQUIRE_FALSE(templated.contains("ttlMs"));

    const auto missing =
        registry.HandleRead(
            Json{
                {"uri",
                 "engine://missing"}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireError(missing).code
        == JsonRpcInvalidParams);
}

TEST_CASE(
    "ResourceRegistry rejects invalid read and list parameters",
    "[mcp][registry][resource][v0.8]")
{
    using namespace Janus::MCP;

    ResourceRegistry registry;

    const auto missingUri =
        registry.HandleRead(
            Json::object(),
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireError(missingUri).code
        == JsonRpcInvalidParams);

    const auto invalidCursor =
        registry.HandleList(
            Json{{"cursor", "-1"}},
            McpProtocolEra::Modern2026);

    REQUIRE(
        RequireError(invalidCursor).code
        == JsonRpcInvalidParams);
}
