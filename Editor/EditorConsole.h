#pragma once

#include "Core/Error/Error.h"
#include "Core/Log/LogStore.h"
#include "Core/Types.h"
#include <memory>

#include <string>
#include <string_view>
#include <vector>

namespace Janus::Editor
{

enum class EditorConsoleLevel
{
    Info,
    Warning,
    Error
};

struct EditorConsoleEntry
{
    EditorConsoleLevel level = EditorConsoleLevel::Info;
    std::string message;
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
    [[nodiscard]] usize GetCapacity() const noexcept;

private:
    void Push(
        EditorConsoleLevel level,
        std::string message);

    std::optional<EditorConsoleLevel> m_LevelFilter;
    usize m_Capacity = 200;
    mutable std::vector<EditorConsoleEntry> m_Entries;
    std::shared_ptr<LogStore> m_Store;
};

} // namespace Janus::Editor
