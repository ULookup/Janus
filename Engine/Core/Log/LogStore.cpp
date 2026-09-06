#include "Core/Log/LogStore.h"
#include <algorithm>
#include <chrono>

namespace Janus
{
std::string_view LogLevelName(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    }
    return "Info";
}
LogStore::LogStore(usize capacity) : m_Capacity(std::clamp<usize>(capacity, 1, 10000)) {}
void LogStore::Append(LogLevel level, std::string category, std::string message, LogContext context)
{
    const bool truncated = message.size() > 8192 || category.size() > 64;
    const auto clip = [](std::string& text, usize limit)
    {
        if (text.size() <= limit)
            return;
        while (limit > 0 && (static_cast<unsigned char>(text[limit]) & 0xC0) == 0x80)
            --limit;
        text.resize(limit);
    };
    clip(message, 8192);
    clip(category, 64);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    std::lock_guard lock(m_Mutex);
    if (m_Entries.size() == m_Capacity)
    {
        m_Entries.pop_front();
        ++m_Dropped;
    }
    m_Entries.push_back({m_NextSequence++, now, level, std::move(category), std::move(message),
                         context, truncated});
}
Result<LogPage> LogStore::Read(const LogQuery& query) const
{
    if (query.limit == 0 || query.limit > 200)
        return Result<LogPage>::Failure(ErrorCode::InvalidArgument, "Log limit must be 1..200.");
    std::lock_guard lock(m_Mutex);
    LogPage page;
    page.oldestSequence = m_Entries.empty() ? m_NextSequence : m_Entries.front().sequence;
    page.droppedCount = m_Dropped;
    const auto matches = [&query](const LogEntry& e)
    {
        return (!query.level || e.level == *query.level) &&
               (query.category.empty() || e.category == query.category) &&
               (!query.runtimeId || e.context.runtimeId == *query.runtimeId);
    };
    u64 after = query.after.value_or(0);
    if (!query.after)
    {
        usize found = 0;
        after = m_NextSequence - 1;
        for (auto it = m_Entries.rbegin(); it != m_Entries.rend(); ++it)
        {
            if (matches(*it))
            {
                after = it->sequence - 1;
                if (++found == query.limit)
                    break;
            }
        }
    }
    page.gap = query.after && after < page.oldestSequence - 1;
    page.nextCursor = std::min(after, m_NextSequence - 1);
    usize bytes = 0;
    for (const auto& entry : m_Entries)
    {
        if (entry.sequence <= after)
            continue;
        if (matches(entry))
        {
            // Resource text is nested JSON: budget both escaping layers plus envelope headroom.
            const usize cost = (entry.message.size() + entry.category.size()) * 12 + 1024;
            if (page.entries.size() == query.limit || bytes + cost > 768 * 1024)
                break;
            page.entries.push_back(entry);
            bytes += cost;
        }
        page.nextCursor = entry.sequence;
    }
    return Result<LogPage>::Success(std::move(page));
}
void LogStore::Clear() noexcept
{
    std::lock_guard lock(m_Mutex);
    m_Dropped += m_Entries.size();
    m_Entries.clear();
}
} // namespace Janus
