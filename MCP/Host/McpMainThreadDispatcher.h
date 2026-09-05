#pragma once

#include "Core/Error/Result.h"
#include "Core/Types.h"
#include "Protocol/McpProtocol.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace Janus::MCP
{

class McpMainThreadDispatcher final
{
public:
    explicit McpMainThreadDispatcher(
        usize maxRequestsPerPump = 8) noexcept;

    ~McpMainThreadDispatcher();

    McpMainThreadDispatcher(const McpMainThreadDispatcher&) = delete;
    McpMainThreadDispatcher& operator=(const McpMainThreadDispatcher&) = delete;

    [[nodiscard]] McpDispatchResult Invoke(
        std::function<McpDispatchResult()> task);

    [[nodiscard]] Result<usize> Pump();

    void Stop() noexcept;

    [[nodiscard]] bool IsOwnerThread() const noexcept;
    [[nodiscard]] bool IsStopped() const noexcept;
    [[nodiscard]] usize GetPendingCount() const noexcept;

private:
    struct PendingRequest
    {
        std::function<McpDispatchResult()> task;
        std::optional<McpDispatchResult> result;
        std::mutex mutex;
        std::condition_variable completed;
    };

    [[nodiscard]] static McpDispatchResult StoppedResult();

    std::thread::id m_OwnerThread;
    usize m_MaxRequestsPerPump = 8;

    mutable std::mutex m_QueueMutex;
    std::deque<std::shared_ptr<PendingRequest>> m_Queue;
    bool m_Stopped = false;
};

} // namespace Janus::MCP
