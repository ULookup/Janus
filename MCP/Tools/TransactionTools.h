#pragma once
#include "Core/Command/CommandBus.h"
#include "Registry/ResourceRegistry.h"
#include "Registry/ToolRegistry.h"
namespace Janus::MCP
{
struct McpTransactionContext
{
    CommandBus* commands = nullptr;
    std::function<Result<UUID>(std::string)> begin;
    std::function<Result<void>(UUID, bool)> finish;
};
Json TransactionStatusJson(const CommandBus& commands);
Result<void> RegisterTransactionCapabilities(ToolRegistry& tools, ResourceRegistry& resources,
                                             McpTransactionContext context);
} // namespace Janus::MCP
