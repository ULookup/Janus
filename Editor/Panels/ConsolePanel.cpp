#include "Panels/ConsolePanel.h"

#include "EditorConsole.h"

#include <imgui.h>

namespace Janus::Editor
{

ConsolePanel::ConsolePanel(
    EditorConsole& console) noexcept
    : m_Console(console)
{
}

void ConsolePanel::DrawContents()
{
    if (ImGui::Button("Clear"))
    {
        m_Console.Clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox(
        "Auto-scroll",
        &m_AutoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::Combo("Level", &m_LevelFilter, "All\0Info\0Warning\0Error\0"))
        m_Console.SetLevelFilter(m_LevelFilter == 0
                                     ? std::nullopt
                                     : std::optional<EditorConsoleLevel>(
                                           static_cast<EditorConsoleLevel>(m_LevelFilter - 1)));
    ImGui::Separator();

    ImGui::BeginChild(
        "ConsoleEntries",
        ImVec2{0.0f, 0.0f},
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar);

    const auto& entries =
        m_Console.GetEntries();

    for (const EditorConsoleEntry& entry : entries)
    {
        const char* prefix = entry.level == EditorConsoleLevel::Error     ? "[Error]"
                             : entry.level == EditorConsoleLevel::Warning ? "[Warning]"
                                                                          : "[Info]";

        ImGui::TextWrapped(
            "%s %s",
            prefix,
            entry.message.c_str());
    }

    if (m_AutoScroll
        && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

} // namespace Janus::Editor
