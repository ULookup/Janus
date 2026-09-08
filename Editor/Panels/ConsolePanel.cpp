#include "Panels/ConsolePanel.h"
#include "EditorLocale.h"

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
    const auto text = [&](const char* key) { return EditorText(m_Language, key); };
    const auto label = [&](const char* key) { return EditorLabel(m_Language, key); };
    if (IconOnlyButton(Icon::Delete, label("Clear console").c_str()))
    {
        m_Console.Clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox(label("Auto-scroll").c_str(), &m_AutoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    const char* levels[]{text("All levels"), text("Info"), text("Warning"), text("Error")};
    if (ImGui::Combo("##Level", &m_LevelFilter, levels, IM_ARRAYSIZE(levels)))
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
        ImGui::TextDisabled(text("No messages at this level."));

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
