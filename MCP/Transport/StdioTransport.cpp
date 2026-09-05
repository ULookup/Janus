#include "Transport/StdioTransport.h"

#include "Protocol/McpProtocol.h"

#include <istream>
#include <ostream>
#include <utility>

namespace Janus::MCP
{

StdioTransport::StdioTransport(
    std::istream& input,
    std::ostream& output,
    usize maxMessageBytes) noexcept
    : m_Input(input),
      m_Output(output),
      m_MaxMessageBytes(maxMessageBytes)
{
}

Result<std::optional<std::string>>
StdioTransport::ReadMessage()
{
    std::string line;
    line.reserve(
        m_MaxMessageBytes < 4096
            ? m_MaxMessageBytes
            : 4096);

    bool exceeded = false;
    char character = '\0';

    while (m_Input.get(character))
    {
        if (character == '\n')
        {
            break;
        }

        if (!exceeded)
        {
            if (line.size()
                >= m_MaxMessageBytes)
            {
                exceeded = true;
            }
            else
            {
                line.push_back(character);
            }
        }
    }

    if (m_Input.bad())
    {
        return Result<std::optional<std::string>>::Failure(
            ErrorCode::FileReadFailed,
            "Failed to read MCP stdio input.");
    }

    if (exceeded)
    {
        return Result<std::optional<std::string>>::Failure(
            ErrorCode::InvalidArgument,
            "MCP stdio message exceeds the configured size limit.");
    }

    if (line.empty()
        && m_Input.eof())
    {
        return Result<std::optional<std::string>>::Success(
            std::nullopt);
    }

    if (!line.empty()
        && line.back() == '\r')
    {
        line.pop_back();
    }

    return Result<std::optional<std::string>>::Success(
        std::move(line));
}

Result<void> StdioTransport::WriteMessage(
    const Json& message)
{
    const std::string encoded =
        message.dump();

    if (encoded.size() > m_MaxMessageBytes)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "MCP stdio response exceeds the configured size limit.");
    }

    if (encoded.find('\n') != std::string::npos
        || encoded.find('\r') != std::string::npos)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "MCP stdio serializer produced an invalid framed message.");
    }

    m_Output
        << encoded
        << '\n';
    m_Output.flush();

    if (!m_Output)
    {
        return Result<void>::Failure(
            ErrorCode::FileWriteFailed,
            "Failed to write MCP stdio output.");
    }

    return Result<void>::Success();
}

Result<void> ServeStdioProtocol(
    StdioTransport& transport,
    McpProtocolSession& session)
{
    while (true)
    {
        auto read =
            transport.ReadMessage();

        if (!read)
        {
            if (read.GetError().code
                == ErrorCode::InvalidArgument)
            {
                auto written =
                    transport.WriteMessage(
                        MakeJsonRpcErrorResponse(
                            std::nullopt,
                            JsonRpcInvalidRequest,
                            read.GetError().message));
                if (!written)
                {
                    return written;
                }

                continue;
            }

            return Result<void>::Failure(
                read.GetError());
        }

        if (!read.Value().has_value())
        {
            return Result<void>::Success();
        }

        auto handled =
            session.HandleMessage(
                *read.Value());
        if (!handled)
        {
            return Result<void>::Failure(
                handled.GetError());
        }

        if (!handled.Value().has_value())
        {
            continue;
        }

        auto written =
            transport.WriteMessage(
                *handled.Value());
        if (!written)
        {
            return written;
        }
    }
}

} // namespace Janus::MCP
