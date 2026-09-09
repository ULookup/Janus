#include "EditorConsole.h"

#include <algorithm>
#include <cctype>
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
    m_Entries = Read().entries;
    return m_Entries;
}

void EditorConsole::SetSearch(std::string_view search)
{
    m_Search.assign(search.substr(0, 256));
    for (char& c : m_Search)
        if (static_cast<unsigned char>(c) < 128)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

EditorConsoleSnapshot EditorConsole::Read() const
{
    EditorConsoleSnapshot view;
    LogQuery query;
    query.after = 0;
    query.limit = 200;
    // Read the bounded store afresh. The view is not a second retained log history.
    usize scanned = 0;
    while (scanned < m_Capacity)
    {
        auto page = m_Store->Read(query);
        if (!page)
            break;
        view.droppedCount = page.Value().droppedCount;
        for (auto& entry : page.Value().entries)
        {
            if (scanned++ == m_Capacity)
                break;
            ++view.counts[static_cast<usize>(entry.level)];
            if ((m_LevelFilter && entry.level != *m_LevelFilter) ||
                (m_RuntimeFilter && entry.context.runtimeId != *m_RuntimeFilter))
                continue;
            if (!m_Search.empty())
            {
                auto haystack = entry.category + "\n" + entry.message;
                for (char& c : haystack)
                    if (static_cast<unsigned char>(c) < 128)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (haystack.find(m_Search) == std::string::npos)
                    continue;
            }
            view.entries.push_back(std::move(entry));
        }
        if (page.Value().entries.empty() || page.Value().nextCursor <= *query.after)
            break;
        query.after = page.Value().nextCursor;
    }
    return view;
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
