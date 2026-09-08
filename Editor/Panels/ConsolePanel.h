#pragma once
#include "EditorLocale.h"

namespace Janus::Editor
{

class EditorConsole;

class ConsolePanel final
{
public:
    explicit ConsolePanel(
        EditorConsole& console) noexcept;

    void DrawContents();
    void SetLanguage(EditorLanguage language) noexcept
    {
        m_Language = language;
    }

private:
    EditorConsole& m_Console;
    EditorLanguage m_Language = EditorLanguage::English;
    int m_LevelFilter = 0;
    bool m_AutoScroll = true;
};

} // namespace Janus::Editor
