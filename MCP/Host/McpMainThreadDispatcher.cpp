#include "Host/McpMainThreadDispatcher.h"

#include <utility>

namespace Janus::MCP
{

McpMainThreadDispatcher::McpMainThreadDispatcher(
    usize maxRequestsPerPump) noexcept
    : m_OwnerThread(std::this_thread::get_id()),
      m_MaxRequestsPerPump(
          maxRequestsPerPump == 0
              ? 1
              : maxRequestsPerPump)
{
}

McpMainThreadDispatcher::~McpMainThreadDispatcher()
{
    Stop();
}

McpDispatchResult McpMainThreadDispatcher::Invoke(
    std::function<McpDispatchResult()> task)
{
    if (!task)
    {
        return McpDispatchError{
            JsonRpcInternalError,
            "MCP main-thread dispatch requires a task.",
            nullptr};
    }

    if (IsOwnerThread())
    {
        if (IsStopped())
        {
            return StoppedResult();
        }

        return task();
    }

    auto pending =
        std::make_shared<PendingRequest>();
    pending->task =
        std::move(task);

    {
        std::lock_guard lock(
            m_QueueMutex);

        if (m_Stopped)
        {
            return StoppedResult();
        }

        m_Queue.push_back(
            pending);
    }

    std::unique_lock pendingLock(
        pending->mutex);

    pending->completed.wait(
        pendingLock,
        [&pending]()
        {
            return pending->result.has_value();
        });

    return std::move(
        *pending->result);
}

Result<usize> McpMainThreadDispatcher::Pump()
{
    if (!IsOwnerThread())
    {
        return Result<usize>::Failure(
            ErrorCode::InvalidState,
            "MCP dispatcher may only be pumped by its owner thread.");
    }

    usize processed = 0;

    while (processed < m_MaxRequestsPerPump)
    {
        std::shared_ptr<PendingRequest> pending;

        {
            std::lock_guard lock(
                m_QueueMutex);

            if (m_Stopped
                || m_Queue.empty())
            {
                break;
            }

            pending =
                std::move(
                    m_Queue.front());
            m_Queue.pop_front();
        }

        McpDispatchResult result =
            pending->task();

        {
            std::lock_guard pendingLock(
                pending->mutex);

            pending->result =
                std::move(result);
        }

        pending->completed.notify_one();
        ++processed;
    }

    return Result<usize>::Success(
        processed);
}

void McpMainThreadDispatcher::Stop() noexcept
{
    std::deque<std::shared_ptr<PendingRequest>> pending;

    {
        std::lock_guard lock(
            m_QueueMutex);

        if (m_Stopped)
        {
            return;
        }

        m_Stopped = true;
        pending.swap(
            m_Queue);
    }

    for (const auto& request : pending)
    {
        {
            std::lock_guard requestLock(
                request->mutex);

            request->result =
                StoppedResult();
        }

        request->completed.notify_one();
    }
}

bool McpMainThreadDispatcher::IsOwnerThread() const noexcept
{
    return std::this_thread::get_id()
        == m_OwnerThread;
}

bool McpMainThreadDispatcher::IsStopped() const noexcept
{
    std::lock_guard lock(
        m_QueueMutex);
    return m_Stopped;
}

usize McpMainThreadDispatcher::GetPendingCount() const noexcept
{
    std::lock_guard lock(
        m_QueueMutex);
    return m_Queue.size();
}

McpDispatchResult McpMainThreadDispatcher::StoppedResult()
{
    return McpDispatchError{
        JsonRpcInternalError,
        "MCP main-thread dispatcher is stopping.",
        nullptr};
}

} // namespace Janus::MCP
