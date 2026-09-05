#pragma once

#include "Core/Error/Error.h"
#include "Core/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace Janus::Editor
{

enum class EditorConsoleLevel
{
    Info,
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

    void PushInfo(std::string message);
    void PushError(const Error& error);
    void Clear() noexcept;

    [[nodiscard]] const std::vector<EditorConsoleEntry>&
        GetEntries() const noexcept;
    [[nodiscard]] usize GetCapacity() const noexcept;

private:
    void Push(
        EditorConsoleLevel level,
        std::string message);

    usize m_Capacity = 200;
    std::vector<EditorConsoleEntry> m_Entries;
};

} // namespace Janus::Editor
