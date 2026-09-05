#include "Protocol/McpProtocol.h"

#include <utility>

namespace Janus::MCP
{
namespace
{

McpDispatchError InvalidParams(std::string message)
{
    return McpDispatchError{
        JsonRpcInvalidParams,
        std::move(message),
        nullptr};
}

McpDispatchError InvalidRequest(std::string message)
{
    return McpDispatchError{
        JsonRpcInvalidRequest,
        std::move(message),
        nullptr};
}

McpDispatchError UnsupportedVersion(std::string message)
{
    return McpDispatchError{
        McpUnsupportedProtocolVersion,
        std::move(message),
        Json{
            {"supportedVersions",
             Json::array(
                 {std::string{
                     McpModernProtocolVersion}})}}};
}

bool HasModernVersionClaim(const Json& params)
{
    if (!params.is_object())
    {
        return false;
    }

    const auto metaIt = params.find("_meta");
    if (metaIt == params.end()
        || !metaIt->is_object())
    {
        return false;
    }

    const auto versionIt =
        metaIt->find(
            std::string{
                McpProtocolVersionMetaKey});

    return versionIt != metaIt->end();
}

bool IsValidImplementationInfo(const Json& value)
{
    if (!value.is_object())
    {
        return false;
    }

    const auto nameIt = value.find("name");
    const auto versionIt = value.find("version");

    return nameIt != value.end()
        && nameIt->is_string()
        && versionIt != value.end()
        && versionIt->is_string();
}

} // namespace

McpProtocolSession::McpProtocolSession(
    McpServerConfig config)
    : m_Config(std::move(config))
{
    if (!m_Config.capabilities.is_object())
    {
        m_Config.capabilities =
            Json::object();
    }
}

void McpProtocolSession::SetRequestHandler(
    McpRequestHandler handler)
{
    m_RequestHandler =
        std::move(handler);
}

void McpProtocolSession::SetNotificationHandler(
    McpNotificationHandler handler)
{
    m_NotificationHandler =
        std::move(handler);
}

Result<std::optional<Json>>
McpProtocolSession::HandleMessage(
    std::string_view message)
{
    const JsonRpcDecodeResult decoded =
        DecodeJsonRpcMessage(
            message,
            m_Config.limits);

    if (const auto* error =
            std::get_if<JsonRpcDecodeError>(
                &decoded);
        error != nullptr)
    {
        return Result<std::optional<Json>>::Success(
            MakeJsonRpcErrorResponse(
                std::nullopt,
                error->code,
                error->message));
    }

    const JsonRpcMessage& rpcMessage =
        std::get<JsonRpcMessage>(decoded);

    if (const auto* request =
            std::get_if<JsonRpcRequest>(
                &rpcMessage);
        request != nullptr)
    {
        return HandleRequest(*request);
    }

    if (const auto* notification =
            std::get_if<JsonRpcNotification>(
                &rpcMessage);
        notification != nullptr)
    {
        return HandleNotification(
            *notification);
    }

    return Result<std::optional<Json>>::Success(
        MakeJsonRpcErrorResponse(
            std::nullopt,
            JsonRpcInvalidRequest,
            "Janus MCP server does not accept client JSON-RPC responses."));
}

Result<std::optional<Json>>
McpProtocolSession::HandleRequest(
    const JsonRpcRequest& request)
{
    if (!request.params.is_object())
    {
        return Result<std::optional<Json>>::Success(
            MakeRequestError(
                request.id,
                InvalidParams(
                    "MCP request params must be an object.")));
    }

    if (m_Era == McpProtocolEra::Unspecified)
    {
        if (request.method == "initialize")
        {
            const auto versionIt =
                request.params.find(
                    "protocolVersion");
            const auto capabilitiesIt =
                request.params.find(
                    "capabilities");
            const auto clientInfoIt =
                request.params.find(
                    "clientInfo");

            if (versionIt == request.params.end()
                || !versionIt->is_string()
                || capabilitiesIt
                    == request.params.end()
                || !capabilitiesIt->is_object()
                || clientInfoIt
                    == request.params.end()
                || !IsValidImplementationInfo(
                    *clientInfoIt))
            {
                return Result<std::optional<Json>>::Success(
                    MakeRequestError(
                        request.id,
                        InvalidParams(
                            "Legacy initialize requires protocolVersion, "
                            "capabilities, and valid clientInfo.")));
            }

            m_Era =
                McpProtocolEra::Legacy2025;
            m_LegacyInitialized = false;

            return Result<std::optional<Json>>::Success(
                MakeJsonRpcSuccessResponse(
                    request.id,
                    BuildLegacyInitializeResult()));
        }

        const auto modernError =
            ValidateModernParams(
                request.params);

        if (modernError.has_value())
        {
            return Result<std::optional<Json>>::Success(
                MakeRequestError(
                    request.id,
                    *modernError));
        }

        m_Era =
            McpProtocolEra::Modern2026;
    }

    if (m_Era == McpProtocolEra::Modern2026)
    {
        if (request.method == "initialize")
        {
            return Result<std::optional<Json>>::Success(
                MakeRequestError(
                    request.id,
                    InvalidRequest(
                        "Legacy initialize is not valid on a modern MCP connection.")));
        }

        if (const auto modernError =
                ValidateModernParams(
                    request.params);
            modernError.has_value())
        {
            return Result<std::optional<Json>>::Success(
                MakeRequestError(
                    request.id,
                    *modernError));
        }

        if (request.method == "server/discover")
        {
            Json result =
                BuildModernDiscoverResult();
            StampModernServerInfo(
                result);

            return Result<std::optional<Json>>::Success(
                MakeJsonRpcSuccessResponse(
                    request.id,
                    std::move(result)));
        }
    }
    else
    {
        if (HasModernVersionClaim(
                request.params))
        {
            return Result<std::optional<Json>>::Success(
                MakeRequestError(
                    request.id,
                    InvalidRequest(
                        "Modern MCP request metadata cannot switch a legacy connection era.")));
        }

        if (!m_LegacyInitialized)
        {
            return Result<std::optional<Json>>::Success(
                MakeRequestError(
                    request.id,
                    InvalidRequest(
                        "Legacy MCP connection has not received notifications/initialized.")));
        }
    }

    if (!m_RequestHandler)
    {
        return Result<std::optional<Json>>::Success(
            MakeJsonRpcErrorResponse(
                request.id,
                JsonRpcMethodNotFound,
                "MCP method not found."));
    }

    McpDispatchResult dispatched =
        m_RequestHandler(
            request.method,
            request.params,
            m_Era);

    if (const auto* error =
            std::get_if<McpDispatchError>(
                &dispatched);
        error != nullptr)
    {
        return Result<std::optional<Json>>::Success(
            MakeRequestError(
                request.id,
                *error));
    }

    Json result =
        std::get<Json>(
            std::move(dispatched));

    if (!result.is_object())
    {
        return Result<std::optional<Json>>::Success(
            MakeJsonRpcErrorResponse(
                request.id,
                JsonRpcInternalError,
                "MCP method handler returned a non-object result."));
    }

    if (m_Era == McpProtocolEra::Modern2026)
    {
        StampModernServerInfo(
            result);
    }

    return Result<std::optional<Json>>::Success(
        MakeJsonRpcSuccessResponse(
            request.id,
            std::move(result)));
}

Result<std::optional<Json>>
McpProtocolSession::HandleNotification(
    const JsonRpcNotification& notification)
{
    if (!notification.params.is_object())
    {
        return Result<std::optional<Json>>::Success(
            std::nullopt);
    }

    if (m_Era == McpProtocolEra::Unspecified)
    {
        if (notification.method
            == "notifications/initialized")
        {
            return Result<std::optional<Json>>::Success(
                std::nullopt);
        }

        if (HasModernVersionClaim(notification.params))
        {
            if (const auto modernError =
                    ValidateModernParams(
                        notification.params);
                modernError.has_value())
            {
                return Result<std::optional<Json>>::Success(
                    std::nullopt);
            }

            m_Era =
                McpProtocolEra::Modern2026;
        }
        else
        {
            return Result<std::optional<Json>>::Success(
                std::nullopt);
        }
    }

    if (m_Era == McpProtocolEra::Legacy2025)
    {
        if (notification.method
            == "notifications/initialized")
        {
            m_LegacyInitialized = true;
            return Result<std::optional<Json>>::Success(
                std::nullopt);
        }

        if (!m_LegacyInitialized)
        {
            return Result<std::optional<Json>>::Success(
                std::nullopt);
        }
    }
    else if (ValidateModernParams(
                 notification.params)
                 .has_value())
    {
        return Result<std::optional<Json>>::Success(
            std::nullopt);
    }

    if (!m_NotificationHandler)
    {
        return Result<std::optional<Json>>::Success(
            std::nullopt);
    }

    auto handled =
        m_NotificationHandler(
            notification.method,
            notification.params,
            m_Era);
    if (!handled)
    {
        return Result<std::optional<Json>>::Failure(
            handled.GetError());
    }

    return Result<std::optional<Json>>::Success(
        std::nullopt);
}

std::optional<McpDispatchError>
McpProtocolSession::ValidateModernParams(
    const Json& params) const
{
    if (!params.is_object())
    {
        return InvalidParams(
            "Modern MCP params must be an object.");
    }

    const auto metaIt =
        params.find("_meta");
    if (metaIt == params.end()
        || !metaIt->is_object())
    {
        return UnsupportedVersion(
            "Modern MCP request requires a protocol metadata envelope.");
    }

    const auto versionIt =
        metaIt->find(
            std::string{
                McpProtocolVersionMetaKey});
    if (versionIt == metaIt->end()
        || !versionIt->is_string()
        || versionIt->get_ref<const std::string&>()
            != McpModernProtocolVersion)
    {
        return UnsupportedVersion(
            "Unsupported or missing MCP protocol version.");
    }

    const auto clientInfoIt =
        metaIt->find(
            std::string{
                McpClientInfoMetaKey});
    if (clientInfoIt != metaIt->end()
        && !IsValidImplementationInfo(
            *clientInfoIt))
    {
        return InvalidParams(
            "MCP clientInfo metadata is malformed.");
    }

    const auto clientCapabilitiesIt =
        metaIt->find(
            std::string{
                McpClientCapabilitiesMetaKey});
    if (clientCapabilitiesIt
            != metaIt->end()
        && !clientCapabilitiesIt->is_object())
    {
        return InvalidParams(
            "MCP clientCapabilities metadata must be an object.");
    }

    return std::nullopt;
}

Json McpProtocolSession::BuildModernDiscoverResult() const
{
    Json result = {
        {"supportedVersions",
         Json::array(
             {std::string{
                 McpModernProtocolVersion}})},
        {"capabilities",
         m_Config.capabilities},
        {"ttlMs", 0},
        {"cacheScope", "private"}};

    if (!m_Config.instructions.empty())
    {
        result["instructions"] =
            m_Config.instructions;
    }

    return result;
}

Json McpProtocolSession::BuildLegacyInitializeResult() const
{
    Json result = {
        {"protocolVersion",
         std::string{
             McpLegacyProtocolVersion}},
        {"capabilities",
         m_Config.capabilities},
        {"serverInfo",
         Json{
             {"name", m_Config.name},
             {"version", m_Config.version}}}};

    if (!m_Config.instructions.empty())
    {
        result["instructions"] =
            m_Config.instructions;
    }

    return result;
}

void McpProtocolSession::StampModernServerInfo(
    Json& result) const
{
    if (!result.is_object())
    {
        return;
    }

    Json& meta =
        result["_meta"];
    if (!meta.is_object())
    {
        meta = Json::object();
    }

    const std::string key{
        McpServerInfoMetaKey};

    if (!meta.contains(key))
    {
        meta[key] = {
            {"name", m_Config.name},
            {"version", m_Config.version}};
    }
}

Json McpProtocolSession::MakeRequestError(
    const JsonRpcId& id,
    const McpDispatchError& error) const
{
    return MakeJsonRpcErrorResponse(
        id,
        error.code,
        error.message,
        error.data);
}

McpProtocolEra McpProtocolSession::GetEra() const noexcept
{
    return m_Era;
}

bool McpProtocolSession::IsLegacyInitialized() const noexcept
{
    return m_LegacyInitialized;
}

} // namespace Janus::MCP
