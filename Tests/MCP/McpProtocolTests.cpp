#include "Protocol/McpProtocol.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace
{

Janus::MCP::Json ModernParams(
    bool includeClientInfo = true)
{
    using namespace Janus::MCP;

    Json meta = {
        {std::string{McpProtocolVersionMetaKey},
         std::string{McpModernProtocolVersion}},
        {std::string{McpClientCapabilitiesMetaKey},
         Json::object()}};

    if (includeClientInfo)
    {
        meta[std::string{McpClientInfoMetaKey}] = {
            {"name", "JanusTestClient"},
            {"version", "1.0"}};
    }

    return Json{
        {"_meta", std::move(meta)}};
}

Janus::MCP::Json RequireResponse(
    Janus::MCP::McpProtocolSession& session,
    const Janus::MCP::Json& request)
{
    auto handled =
        session.HandleMessage(request.dump());

    REQUIRE(handled);
    REQUIRE(handled.Value().has_value());
    return *handled.Value();
}

Janus::MCP::Json LegacyInitialize(
    std::string protocolVersion = "2025-11-25")
{
    return Janus::MCP::Json{
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params",
         Janus::MCP::Json{
             {"protocolVersion", std::move(protocolVersion)},
             {"capabilities", Janus::MCP::Json::object()},
             {"clientInfo",
              Janus::MCP::Json{
                  {"name", "LegacyClient"},
                  {"version", "1.0"}}}}}};
}

} // namespace

TEST_CASE(
    "Modern MCP discovery selects the 2026 protocol era",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpServerConfig config;
    config.name = "Janus";
    config.version = "0.8-test";
    config.capabilities = {
        {"tools", Json::object()},
        {"resources", Json::object()}};

    McpProtocolSession session(std::move(config));

    const Json response = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", 7},
            {"method", "server/discover"},
            {"params", ModernParams()}});

    REQUIRE(session.GetEra() == McpProtocolEra::Modern2026);
    REQUIRE(response.at("id") == 7);

    const Json& result = response.at("result");
    REQUIRE(result.at("supportedVersions").at(0)
            == std::string{McpModernProtocolVersion});
    REQUIRE(result.at("resultType") == "complete");
    REQUIRE(result.at("ttlMs") == 0);
    REQUIRE(result.at("cacheScope") == "private");
    REQUIRE(result.at("capabilities").contains("tools"));
    REQUIRE(result.at("capabilities").contains("resources"));

    const Json& serverInfo =
        result.at("_meta")
            .at(std::string{McpServerInfoMetaKey});

    REQUIRE(serverInfo.at("name") == "Janus");
    REQUIRE(serverInfo.at("version") == "0.8-test");
    REQUIRE_FALSE(result.contains("serverInfo"));
}

TEST_CASE(
    "Modern MCP validates protocol envelope before dispatch",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    SECTION("missing metadata")
    {
        McpProtocolSession session;

        const Json response = RequireResponse(
            session,
            Json{
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "tools/list"},
                {"params", Json::object()}});

        REQUIRE(
            response.at("error").at("code")
            == McpUnsupportedProtocolVersion);
        REQUIRE(
            session.GetEra()
            == McpProtocolEra::Unspecified);
    }

    SECTION("unsupported modern revision")
    {
        McpProtocolSession session;
        Json params = ModernParams();
        params["_meta"][
            std::string{McpProtocolVersionMetaKey}] =
            "2099-01-01";

        const Json response = RequireResponse(
            session,
            Json{
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "tools/list"},
                {"params", std::move(params)}});

        REQUIRE(
            response.at("error").at("code")
            == McpUnsupportedProtocolVersion);
        REQUIRE(
            session.GetEra()
            == McpProtocolEra::Unspecified);
    }

    SECTION("modern clientInfo is optional")
    {
        McpProtocolSession session;

        const Json response = RequireResponse(
            session,
            Json{
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "server/discover"},
                {"params", ModernParams(false)}});

        REQUIRE(response.contains("result"));
        REQUIRE(
            session.GetEra()
            == McpProtocolEra::Modern2026);
    }

    SECTION("malformed optional clientInfo")
    {
        McpProtocolSession session;
        Json params = ModernParams();
        params["_meta"][
            std::string{McpClientInfoMetaKey}] =
            "bad";

        const Json response = RequireResponse(
            session,
            Json{
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "server/discover"},
                {"params", std::move(params)}});

        REQUIRE(
            response.at("error").at("code")
            == JsonRpcInvalidParams);
        REQUIRE(
            session.GetEra()
            == McpProtocolEra::Unspecified);
    }
}

TEST_CASE(
    "Modern MCP dispatch stamps server identity on handler results",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;
    int callCount = 0;

    session.SetRequestHandler(
        [&callCount](
            std::string_view method,
            const Json& params,
            McpProtocolEra era) -> McpDispatchResult
        {
            ++callCount;
            REQUIRE(method == "tools/list");
            REQUIRE(era == McpProtocolEra::Modern2026);
            REQUIRE_FALSE(params.contains("_meta"));

            return Json{
                {"tools", Json::array()}};
        });

    const Json response = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", "modern-1"},
            {"method", "tools/list"},
            {"params", ModernParams()}});

    REQUIRE(callCount == 1);
    REQUIRE(response.at("id") == "modern-1");
    REQUIRE(response.at("result").contains("tools"));
    REQUIRE(response.at("result").at("resultType") == "complete");
    REQUIRE(
        response.at("result")
            .at("_meta")
            .contains(
                std::string{McpServerInfoMetaKey}));
}

TEST_CASE(
    "Modern MCP does not allow legacy initialize to switch era",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;

    REQUIRE(
        RequireResponse(
            session,
            Json{
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "server/discover"},
                {"params", ModernParams()}})
            .contains("result"));

    const Json response =
        RequireResponse(
            session,
            LegacyInitialize());

    REQUIRE(
        response.at("error").at("code")
        == JsonRpcInvalidRequest);
    REQUIRE(
        session.GetEra()
        == McpProtocolEra::Modern2026);
}

TEST_CASE(
    "Legacy MCP initialize counter-offers the supported 2025 revision",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;

    const Json response =
        RequireResponse(
            session,
            LegacyInitialize("2025-06-18"));

    REQUIRE(
        session.GetEra()
        == McpProtocolEra::Legacy2025);
    REQUIRE_FALSE(
        session.IsLegacyInitialized());

    REQUIRE(
        response.at("result").at("protocolVersion")
        == std::string{McpLegacyProtocolVersion});
    REQUIRE(
        response.at("result")
            .at("serverInfo")
            .at("name")
        == "Janus");
}

TEST_CASE(
    "Legacy MCP blocks requests until initialized notification",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;
    int calls = 0;

    session.SetRequestHandler(
        [&calls](
            std::string_view,
            const Json&,
            McpProtocolEra era) -> McpDispatchResult
        {
            ++calls;
            REQUIRE(era == McpProtocolEra::Legacy2025);
            return Json{
                {"tools", Json::array()}};
        });

    REQUIRE(
        RequireResponse(
            session,
            LegacyInitialize())
            .contains("result"));

    const Json early = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/list"},
            {"params", Json::object()}});

    REQUIRE(
        early.at("error").at("code")
        == JsonRpcInvalidRequest);
    REQUIRE(calls == 0);

    auto initialized =
        session.HandleMessage(
            Json{
                {"jsonrpc", "2.0"},
                {"method", "notifications/initialized"}}
                .dump());

    REQUIRE(initialized);
    REQUIRE_FALSE(
        initialized.Value().has_value());
    REQUIRE(
        session.IsLegacyInitialized());

    const Json response = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", 3},
            {"method", "tools/list"},
            {"params", Json::object()}});

    REQUIRE(response.at("result").contains("tools"));
    REQUIRE(calls == 1);
}

TEST_CASE(
    "Legacy MCP rejects modern metadata era switching",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;

    REQUIRE(
        RequireResponse(
            session,
            LegacyInitialize())
            .contains("result"));

    REQUIRE(
        session.HandleMessage(
            Json{
                {"jsonrpc", "2.0"},
                {"method", "notifications/initialized"}}
                .dump()));

    const Json response = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", 4},
            {"method", "tools/list"},
            {"params", ModernParams()}});

    REQUIRE(
        response.at("error").at("code")
        == JsonRpcInvalidRequest);
    REQUIRE(
        session.GetEra()
        == McpProtocolEra::Legacy2025);
}

TEST_CASE(
    "Claim-less notifications do not accidentally select an MCP era",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;

    auto handled =
        session.HandleMessage(
            Json{
                {"jsonrpc", "2.0"},
                {"method", "notifications/progress"},
                {"params", Json{{"progress", 1}}}}
                .dump());

    REQUIRE(handled);
    REQUIRE_FALSE(
        handled.Value().has_value());
    REQUIRE(
        session.GetEra()
        == McpProtocolEra::Unspecified);
}

TEST_CASE(
    "MCP protocol converts invalid JSON into a JSON-RPC parse error",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;

    auto handled =
        session.HandleMessage("{");

    REQUIRE(handled);
    REQUIRE(handled.Value().has_value());

    const Json& response =
        *handled.Value();

    REQUIRE(response.at("id").is_null());
    REQUIRE(
        response.at("error").at("code")
        == JsonRpcParseError);
}


TEST_CASE(
    "MCP protocol returns method not found without a registered handler",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;

    const Json response = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", 11},
            {"method", "tools/list"},
            {"params", ModernParams()}});

    REQUIRE(
        response.at("error").at("code")
        == JsonRpcMethodNotFound);
    REQUIRE(
        session.GetEra()
        == McpProtocolEra::Modern2026);
}


TEST_CASE(
    "Legacy MCP drops modern notifications after era pinning",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;
    int notifications = 0;

    session.SetNotificationHandler(
        [&notifications](
            std::string_view,
            const Json&,
            McpProtocolEra) -> Janus::Result<void>
        {
            ++notifications;
            return Janus::Result<void>::Success();
        });

    REQUIRE(
        RequireResponse(
            session,
            LegacyInitialize())
            .contains("result"));

    REQUIRE(
        session.HandleMessage(
            Json{
                {"jsonrpc", "2.0"},
                {"method", "notifications/initialized"}}
                .dump()));

    auto dropped =
        session.HandleMessage(
            Json{
                {"jsonrpc", "2.0"},
                {"method", "notifications/progress"},
                {"params", ModernParams()}}
                .dump());

    REQUIRE(dropped);
    REQUIRE_FALSE(dropped.Value().has_value());
    REQUIRE(notifications == 0);
    REQUIRE(
        session.GetEra()
        == McpProtocolEra::Legacy2025);
}


TEST_CASE(
    "Modern MCP lifts reserved metadata but preserves application metadata",
    "[mcp][protocol][v0.8]")
{
    using namespace Janus::MCP;

    McpProtocolSession session;
    bool dispatched = false;

    session.SetRequestHandler(
        [&dispatched](
            std::string_view,
            const Json& params,
            McpProtocolEra) -> McpDispatchResult
        {
            dispatched = true;
            REQUIRE(params.contains("_meta"));
            REQUIRE(params.at("_meta").at("trace") == "keep");
            REQUIRE_FALSE(
                params.at("_meta").contains(
                    std::string{McpProtocolVersionMetaKey}));
            REQUIRE_FALSE(
                params.at("_meta").contains(
                    std::string{McpClientInfoMetaKey}));
            REQUIRE_FALSE(
                params.at("_meta").contains(
                    std::string{McpClientCapabilitiesMetaKey}));
            REQUIRE_FALSE(
                params.at("_meta").contains(
                    std::string{McpLogLevelMetaKey}));

            return Json{{"ok", true}};
        });

    Json params = ModernParams();
    params["_meta"]["trace"] = "keep";
    params["_meta"][std::string{McpLogLevelMetaKey}] = "info";

    const Json response = RequireResponse(
        session,
        Json{
            {"jsonrpc", "2.0"},
            {"id", 20},
            {"method", "custom/test"},
            {"params", std::move(params)}});

    REQUIRE(dispatched);
    REQUIRE(response.at("result").at("ok") == true);
}
