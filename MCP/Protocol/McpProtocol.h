#pragma once

#include "Core/Error/Result.h"
#include "Protocol/JsonRpc.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace Janus::MCP
{

inline constexpr std::string_view McpModernProtocolVersion =
    "2026-07-28";
inline constexpr std::string_view McpLegacyProtocolVersion =
    "2025-11-25";

inline constexpr std::string_view McpProtocolVersionMetaKey =
    "io.modelcontextprotocol/protocolVersion";
inline constexpr std::string_view McpClientInfoMetaKey =
    "io.modelcontextprotocol/clientInfo";
inline constexpr std::string_view McpClientCapabilitiesMetaKey =
    "io.modelcontextprotocol/clientCapabilities";
inline constexpr std::string_view McpServerInfoMetaKey =
    "io.modelcontextprotocol/serverInfo";

inline constexpr i32 McpUnsupportedProtocolVersion = -32022;

enum class McpProtocolEra
{
    Unspecified,
    Modern2026,
    Legacy2025
};

struct McpServerConfig
{
    std::string name = "Janus";
    std::string version = "0.1.0";
    std::string instructions;
    Json capabilities = Json::object();
    JsonRpcLimits limits;
};

struct McpDispatchError
{
    i32 code = JsonRpcInternalError;
    std::string message;
    Json data = nullptr;
};

using McpDispatchResult =
    std::variant<Json, McpDispatchError>;

using McpRequestHandler =
    std::function<McpDispatchResult(
        std::string_view method,
        const Json& params,
        McpProtocolEra era)>;

using McpNotificationHandler =
    std::function<Result<void>(
        std::string_view method,
        const Json& params,
        McpProtocolEra era)>;

class McpProtocolSession final
{
public:
    explicit McpProtocolSession(
        McpServerConfig config = {});

    void SetRequestHandler(
        McpRequestHandler handler);

    void SetNotificationHandler(
        McpNotificationHandler handler);

    [[nodiscard]] Result<std::optional<Json>> HandleMessage(
        std::string_view message);

    [[nodiscard]] McpProtocolEra GetEra() const noexcept;
    [[nodiscard]] bool IsLegacyInitialized() const noexcept;

private:
    [[nodiscard]] Result<std::optional<Json>> HandleRequest(
        const JsonRpcRequest& request);

    [[nodiscard]] Result<std::optional<Json>> HandleNotification(
        const JsonRpcNotification& notification);

    [[nodiscard]] std::optional<McpDispatchError>
    ValidateModernParams(
        const Json& params) const;

    [[nodiscard]] Json BuildModernDiscoverResult() const;
    [[nodiscard]] Json BuildLegacyInitializeResult() const;

    void FinalizeModernResult(Json& result) const;

    [[nodiscard]] Json MakeRequestError(
        const JsonRpcId& id,
        const McpDispatchError& error) const;

private:
    McpServerConfig m_Config;
    McpProtocolEra m_Era =
        McpProtocolEra::Unspecified;
    bool m_LegacyInitialized = false;

    McpRequestHandler m_RequestHandler;
    McpNotificationHandler m_NotificationHandler;
};

} // namespace Janus::MCP
