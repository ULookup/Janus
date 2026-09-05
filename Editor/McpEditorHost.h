#pragma once

#include "Host/McpMainThreadDispatcher.h"
#include "Host/McpPermissionPolicy.h"
#include "Protocol/McpProtocol.h"
#include "Registry/McpCapabilityRouter.h"
#include "Registry/ResourceRegistry.h"
#include "Registry/ToolRegistry.h"
#include "Transport/StdioTransport.h"

#include <atomic>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace Janus::Editor
{

class ProjectSession;

class McpEditorHost final
{
public:
    [[nodiscard]] static Result<std::unique_ptr<McpEditorHost>> Create(
        ProjectSession& project,
        std::istream& input,
        std::ostream& output,
        const MCP::IMcpPermissionPolicy& permissionPolicy,
        usize maxRequestsPerPump = 8);

    ~McpEditorHost();

    McpEditorHost(const McpEditorHost&) = delete;
    McpEditorHost& operator=(const McpEditorHost&) = delete;
    McpEditorHost(McpEditorHost&&) = delete;
    McpEditorHost& operator=(McpEditorHost&&) = delete;

    [[nodiscard]] Result<void> Start();
    [[nodiscard]] Result<usize> Pump();
    void Stop() noexcept;

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] std::optional<Error> GetWorkerError() const;

private:
    McpEditorHost(
        ProjectSession& project,
        std::istream& input,
        std::ostream& output,
        const MCP::IMcpPermissionPolicy& permissionPolicy,
        usize maxRequestsPerPump);

    [[nodiscard]] Result<void> RegisterCapabilities();
    [[nodiscard]] MCP::McpDispatchResult DispatchRequest(
        std::string_view method,
        const MCP::Json& params,
        MCP::McpProtocolEra era);

    void RunWorker() noexcept;
    void InterruptWorkerRead() noexcept;
    void RecordWorkerError(Error error) noexcept;

    ProjectSession& m_Project;
    const MCP::IMcpPermissionPolicy& m_PermissionPolicy;

    MCP::ToolRegistry m_Tools;
    MCP::ResourceRegistry m_Resources;
    MCP::McpCapabilityRouter m_Router;
    MCP::McpMainThreadDispatcher m_Dispatcher;
    MCP::McpProtocolSession m_Protocol;
    MCP::StdioTransport m_Transport;

    std::thread m_Worker;
    std::atomic<bool> m_Running{false};
    std::atomic<bool> m_Stopping{false};

    mutable std::mutex m_ErrorMutex;
    std::optional<Error> m_WorkerError;
};

} // namespace Janus::Editor
