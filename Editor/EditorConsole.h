#pragma once

#include "Core/Error/Error.h"
#include "Core/Log/LogStore.h"
#include "Core/Types.h"
#include <array>
#include <memory>

#include <string>
#include <string_view>
#include <vector>

namespace Janus::Editor
{

using EditorConsoleLevel = LogLevel;
using EditorConsoleEntry = LogEntry;

struct EditorConsoleSnapshot
{
    std::vector<LogEntry> entries;
    std::array<usize, 3> counts{};
    u64 droppedCount = 0;
};

class EditorConsole final
{
public:
    explicit EditorConsole(usize capacity = 200);
    explicit EditorConsole(std::shared_ptr<LogStore> store);

    void PushInfo(std::string message);
    void PushError(const Error& error);
    void Clear() noexcept;
    void SetLevelFilter(std::optional<EditorConsoleLevel> level) noexcept
    {
        m_LevelFilter = level;
    }

    [[nodiscard]] const std::vector<EditorConsoleEntry>& GetEntries() const;
    [[nodiscard]] EditorConsoleSnapshot Read() const;
    void SetSearch(std::string_view search);
    void SetRuntimeFilter(std::optional<UUID> runtime) noexcept
    {
        m_RuntimeFilter = runtime;
    }
    [[nodiscard]] usize GetCapacity() const noexcept;

private:
    void Push(
        EditorConsoleLevel level,
        std::string message);

    std::optional<EditorConsoleLevel> m_LevelFilter;
    std::optional<UUID> m_RuntimeFilter;
    std::string m_Search;
    usize m_Capacity = 200;
    mutable std::vector<EditorConsoleEntry> m_Entries;
    std::shared_ptr<LogStore> m_Store;
};

} // namespace Janus::Editor
