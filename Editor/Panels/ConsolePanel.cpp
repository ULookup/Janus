#include "Panels/ConsolePanel.h"

#include "EditorConsole.h"
#include "EditorIcons.h"

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
    if (IconButton(Icon::Delete, "Clear"))
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

        const ImVec4 color = entry.level == EditorConsoleLevel::Error ? ImVec4{1, .40f, .40f, 1}
                             : entry.level == EditorConsoleLevel::Warning
                                 ? ImVec4{1, .78f, .30f, 1}
                                 : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        const float iconSize = ImGui::GetFontSize();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy({iconSize, iconSize});
        DrawIcon(entry.level == EditorConsoleLevel::Error     ? Icon::Error
                 : entry.level == EditorConsoleLevel::Warning ? Icon::Warning
                                                              : Icon::Info,
                 origin, iconSize);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextWrapped(
            "%s %s",
            prefix,
            entry.message.c_str());
        ImGui::PopStyleColor();
    }

    if (m_AutoScroll
        && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

} // namespace Janus::Editor
