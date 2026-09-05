#pragma once

#include "Core/Error/Result.h"
#include "Core/Types.h"
#include "Protocol/JsonRpc.h"

#include <iosfwd>
#include <optional>
#include <string>

namespace Janus::MCP
{

class McpProtocolSession;

class StdioTransport final
{
public:
    StdioTransport(
        std::istream& input,
        std::ostream& output,
        usize maxMessageBytes =
            JsonRpcLimits{}.maxMessageBytes) noexcept;

    [[nodiscard]] Result<std::optional<std::string>>
    ReadMessage();

    [[nodiscard]] Result<void> WriteMessage(
        const Json& message);

private:
    std::istream& m_Input;
    std::ostream& m_Output;
    usize m_MaxMessageBytes;
};

[[nodiscard]] Result<void> ServeStdioProtocol(
    StdioTransport& transport,
    McpProtocolSession& session);

} // namespace Janus::MCP
