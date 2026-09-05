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

void ConsolePanel::Draw()
{
    const bool visible = ImGui::Begin(
        "Console",
        nullptr,
        ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse);

    if (!visible)
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
    {
        m_Console.Clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox(
        "Auto-scroll",
        &m_AutoScroll);

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
        const char* prefix =
            entry.level == EditorConsoleLevel::Error
                ? "[Error]"
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
    ImGui::End();
}

} // namespace Janus::Editor
