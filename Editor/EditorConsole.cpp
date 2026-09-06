#include "EditorConsole.h"

#include <algorithm>
#include <utility>

namespace Janus::Editor
{

EditorConsole::EditorConsole(usize capacity)
    : m_Capacity(std::max<usize>(capacity, 1)), m_Store(std::make_shared<LogStore>(m_Capacity))
{
    m_Entries.reserve(m_Capacity);
}

EditorConsole::EditorConsole(std::shared_ptr<LogStore> store)
    : m_Capacity(store ? store->GetCapacity() : 200),
      m_Store(store ? std::move(store) : std::make_shared<LogStore>(200))
{
}

void EditorConsole::PushInfo(std::string message)
{
    Push(
        EditorConsoleLevel::Info,
        std::move(message));
}

void EditorConsole::PushError(const Error& error)
{
    Push(
        EditorConsoleLevel::Error,
        error.message);
}

void EditorConsole::Clear() noexcept
{
    m_Entries.clear();
    m_Store->Clear();
}

const std::vector<EditorConsoleEntry>& EditorConsole::GetEntries() const
{
    LogQuery query;
    query.limit = std::min<usize>(m_Capacity, 200);
    if (m_LevelFilter)
        query.level = *m_LevelFilter == EditorConsoleLevel::Error     ? LogLevel::Error
                      : *m_LevelFilter == EditorConsoleLevel::Warning ? LogLevel::Warning
                                                                      : LogLevel::Info;
    const auto page = m_Store->Read(query);
    m_Entries.clear();
    if (page)
        for (const auto& entry : page.Value().entries)
            m_Entries.push_back({entry.level == LogLevel::Error     ? EditorConsoleLevel::Error
                                 : entry.level == LogLevel::Warning ? EditorConsoleLevel::Warning
                                                                    : EditorConsoleLevel::Info,
                                 entry.message});
    return m_Entries;
}

usize EditorConsole::GetCapacity() const noexcept
{
    return m_Capacity;
}

void EditorConsole::Push(
    EditorConsoleLevel level,
    std::string message)
{
    m_Store->Append(level == EditorConsoleLevel::Error ? LogLevel::Error : LogLevel::Info, "Editor",
                    std::move(message));
}

} // namespace Janus::Editor
