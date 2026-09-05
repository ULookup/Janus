#include "EditorConsole.h"

#include <algorithm>
#include <utility>

namespace Janus::Editor
{

EditorConsole::EditorConsole(usize capacity)
    : m_Capacity(std::max<usize>(capacity, 1))
{
    m_Entries.reserve(m_Capacity);
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
}

const std::vector<EditorConsoleEntry>&
EditorConsole::GetEntries() const noexcept
{
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
    if (m_Entries.size() == m_Capacity)
    {
        m_Entries.erase(m_Entries.begin());
    }

    m_Entries.push_back(
        EditorConsoleEntry{
            level,
            std::move(message)});
}

} // namespace Janus::Editor
