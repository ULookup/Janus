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
    bool m_AutoScroll = true;
};

} // namespace Janus::Editor
