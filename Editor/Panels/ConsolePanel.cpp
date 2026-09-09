#include "Panels/ConsolePanel.h"
#include "EditorConsole.h"
#include "EditorIcons.h"
#include "EditorLocale.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <imgui.h>

namespace Janus::Editor
{
namespace
{
std::string LogTime(i64 milliseconds)
{
    const auto seconds = static_cast<std::time_t>(milliseconds / 1000);
    std::tm utc{};
    gmtime_s(&utc, &seconds);
    char buffer[40]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d UTC",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min,
                  utc.tm_sec, static_cast<int>(milliseconds % 1000));
    return buffer;
}
} // namespace

ConsolePanel::ConsolePanel(EditorConsole& console) noexcept : m_Console(console) {}

void ConsolePanel::FocusRuntimeError(UUID runtime, std::string_view message)
{
    m_FocusMessage.assign(message);
    m_RuntimeFilter = runtime;
    m_LevelFilter = 3;
    m_Search.fill(0);
    m_Console.SetSearch("");
    m_Console.SetLevelFilter(EditorConsoleLevel::Error);
    m_Console.SetRuntimeFilter(runtime);
    m_SelectedSequence.reset();
    m_FocusError = true;
}

void ConsolePanel::DrawContents()
{
    const auto text = [&](const char* key) { return EditorText(m_Language, key); };
    const auto label = [&](const char* key) { return EditorLabel(m_Language, key); };
    if (IconOnlyButton(Icon::Delete, label("Clear console").c_str()))
    {
        m_Console.Clear();
        m_SelectedSequence.reset();
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
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##LogSearch", text("Search messages or categories..."),
                                 m_Search.data(), m_Search.size()))
        m_Console.SetSearch(m_Search.data());
    if (m_RuntimeFilter)
    {
        ImGui::TextWrapped("%s: %s", text("Runtime"), m_RuntimeFilter->ToString().c_str());
        if (ImGui::SmallButton(label("Show all runtimes").c_str()))
        {
            m_RuntimeFilter.reset();
            m_Console.SetRuntimeFilter({});
        }
    }
    const auto view = m_Console.Read();
    ImGui::TextWrapped(
        text("Retained: %zu info / %zu warnings / %zu errors | Showing %zu | Dropped %llu"),
        view.counts[0], view.counts[1], view.counts[2], view.entries.size(), view.droppedCount);
    if (m_FocusError)
    {
        const auto error =
            std::find_if(view.entries.rbegin(), view.entries.rend(),
                         [&](const auto& entry) { return entry.message == m_FocusMessage; });
        if (error != view.entries.rend())
            m_SelectedSequence = error->sequence;
        m_FocusMessage.clear();
    }
    auto selected = std::find_if(view.entries.begin(), view.entries.end(), [&](const auto& entry)
                                 { return m_SelectedSequence == entry.sequence; });
    if (selected == view.entries.end())
        m_SelectedSequence.reset();

    ImGui::BeginChild("ConsoleEntries", {0, 0}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (view.entries.empty())
        ImGui::TextDisabled("%s", text("No matching messages."));
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(view.entries.size()), ImGui::GetFrameHeightWithSpacing());
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const auto& entry = view.entries[static_cast<usize>(i)];
            const auto color = entry.level == LogLevel::Error ? ImVec4{1, .40f, .40f, 1}
                               : entry.level == LogLevel::Warning
                                   ? ImVec4{1, .78f, .30f, 1}
                                   : ImGui::GetStyleColorVec4(ImGuiCol_Text);
            const auto row =
                LogTime(entry.timestampMilliseconds) + " [" + entry.category + "] " + entry.message;
            ImGui::PushID(std::to_string(entry.sequence).c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            const auto origin = ImGui::GetCursorScreenPos();
            const auto width = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##Log", m_SelectedSequence == entry.sequence, 0,
                                  {width, ImGui::GetFrameHeight()}))
                m_SelectedSequence = entry.sequence;
            DrawEllipsizedText(ImGui::GetWindowDrawList(), origin, width, ImGui::GetColorU32(color),
                               row);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    if (m_FocusError || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f))
        ImGui::SetScrollHereY(1.0f);
    m_FocusError = false;
    ImGui::EndChild();

    // Resolve the identity against this frame's store view, never keep a copied stale detail.
    selected = std::find_if(view.entries.begin(), view.entries.end(), [&](const auto& entry)
                            { return m_SelectedSequence == entry.sequence; });
    if (selected != view.entries.end())
    {
        const auto* viewport = ImGui::GetMainViewport();
        const float scale = ImGui::GetFontSize() / 17.0f;
        const ImVec2 size{std::min(560 * scale, viewport->WorkSize.x - 24),
                          std::min(360 * scale, viewport->WorkSize.y - 24)};
        ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
        bool open = true;
        const bool visible =
            ImGui::Begin(label("Log details").c_str(), &open, ImGuiWindowFlags_NoSavedSettings);
        if (visible)
        {
            if (ImGui::SmallButton(label("Copy message").c_str()))
                ImGui::SetClipboardText(selected->message.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(label("Close details").c_str()))
                m_SelectedSequence.reset();
            ImGui::TextWrapped("#%llu | %s | %s | %s", selected->sequence,
                               LogTime(selected->timestampMilliseconds).c_str(),
                               text(LogLevelName(selected->level).data()),
                               selected->category.c_str());
            if (selected->context.runtimeId.IsValid())
                ImGui::TextWrapped("%s: %s", text("Runtime"),
                                   selected->context.runtimeId.ToString().c_str());
            if (selected->context.frameIndex)
                ImGui::Text("%s: %llu", text("Frame"), *selected->context.frameIndex);
            if (selected->context.errorCode)
                ImGui::Text("%s: %d", text("Error code"),
                            static_cast<int>(*selected->context.errorCode));
            if (selected->truncated)
                ImGui::TextDisabled("%s", text("Message truncated by log capacity limits."));
            ImGui::TextWrapped("%s", selected->message.c_str());
        }
        ImGui::End();
        if (!open)
            m_SelectedSequence.reset();
    }
}
} // namespace Janus::Editor
