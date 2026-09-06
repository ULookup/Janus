#pragma once

namespace Janus::Editor
{

class EditorConsole;

class ConsolePanel final
{
public:
    explicit ConsolePanel(
        EditorConsole& console) noexcept;

    void DrawContents();

private:
    EditorConsole& m_Console;
    int m_LevelFilter = 0;
    bool m_AutoScroll = true;
};

} // namespace Janus::Editor
