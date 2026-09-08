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
    if (IconOnlyButton(Icon::Delete, "Clear console"))
    {
        m_Console.Clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox(
        "Auto-scroll",
        &m_AutoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    if (ImGui::Combo("##Level", &m_LevelFilter, "All levels\0Info\0Warning\0Error\0"))
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

    if (entries.empty())
        ImGui::TextDisabled("No messages at this level.");

    int row = 0;
    for (const EditorConsoleEntry& entry : entries)
    {
        const ImVec4 color = entry.level == EditorConsoleLevel::Error ? ImVec4{1, .40f, .40f, 1}
                             : entry.level == EditorConsoleLevel::Warning
                                 ? ImVec4{1, .78f, .30f, 1}
                                 : ImGui::GetStyleColorVec4(ImGuiCol_Text);
        const float iconSize = ImGui::GetFontSize();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float height = ImGui::GetFrameHeight();
        auto* draw = ImGui::GetWindowDrawList();
        if (row % 2 == 0)
            draw->AddRectFilled(origin, {origin.x + width, origin.y + height},
                                IM_COL32(255, 255, 255, 5));
        ImGui::PushID(row++);
        ImGui::Selectable("##Message", false, 0, {width, height});
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40);
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopID();
        DrawIcon(entry.level == EditorConsoleLevel::Error     ? Icon::Error
                 : entry.level == EditorConsoleLevel::Warning ? Icon::Warning
                                                              : Icon::Info,
                 {origin.x + 4, origin.y + (height - iconSize) * .5f}, iconSize);
        const float inset = iconSize + ImGui::GetStyle().ItemSpacing.x + 4;
        DrawEllipsizedText(draw, {origin.x + inset, origin.y + (height - iconSize) * .5f},
                           width - inset, ImGui::GetColorU32(color), entry.message);
    }

    if (m_AutoScroll
        && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

} // namespace Janus::Editor
