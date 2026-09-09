#pragma once
#include "Core/UUID/UUID.h"
#include "EditorLocale.h"
#include <array>
#include <optional>

namespace Janus::Editor
{

class EditorConsole;

class ConsolePanel final
{
public:
    explicit ConsolePanel(
        EditorConsole& console) noexcept;

    void DrawContents();
    void FocusRuntimeError(UUID runtime, std::string_view message);
    void SetLanguage(EditorLanguage language) noexcept
    {
        m_Language = language;
    }

private:
    EditorConsole& m_Console;
    EditorLanguage m_Language = EditorLanguage::English;
    int m_LevelFilter = 0;
    bool m_AutoScroll = true;
    std::array<char, 257> m_Search{};
    std::optional<UUID> m_RuntimeFilter;
    std::optional<u64> m_SelectedSequence;
    bool m_FocusError = false;
    std::string m_FocusMessage;
};

} // namespace Janus::Editor
