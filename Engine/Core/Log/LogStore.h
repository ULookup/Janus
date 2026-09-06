#pragma once
#include "Core/Error/Result.h"
#include "Core/UUID/UUID.h"
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Janus
{
enum class LogLevel
{
    Info,
    Warning,
    Error
};
[[nodiscard]] std::string_view LogLevelName(LogLevel level) noexcept;
struct LogContext
{
    UUID runtimeId;
    std::optional<u64> frameIndex;
    std::optional<ErrorCode> errorCode;
};
struct LogEntry
{
    u64 sequence = 0;
    i64 timestampMilliseconds = 0;
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
    LogContext context;
    bool truncated = false;
};
struct LogQuery
{
    std::optional<u64> after;
    usize limit = 100;
    std::optional<LogLevel> level;
    std::string category;
    std::optional<UUID> runtimeId;
};
struct LogPage
{
    std::vector<LogEntry> entries;
    u64 oldestSequence = 1;
    u64 nextCursor = 0;
    u64 droppedCount = 0;
    bool gap = false;
};
class LogStore final
{
  public:
    explicit LogStore(usize capacity = 2000);
    void Append(LogLevel level, std::string category, std::string message, LogContext context = {});
    [[nodiscard]] Result<LogPage> Read(const LogQuery& query = {}) const;
    void Clear() noexcept;
    [[nodiscard]] usize GetCapacity() const noexcept
    {
        return m_Capacity;
    }

  private:
    usize m_Capacity;
    mutable std::mutex m_Mutex;
    std::deque<LogEntry> m_Entries;
    u64 m_NextSequence = 1;
    u64 m_Dropped = 0;
};
} // namespace Janus
