#include "Panels/ProjectSettingsPanel.h"
#include "EditorLocale.h"
#include "ProjectSession.h"
#include <algorithm>
#include <imgui.h>
#include <optional>

namespace Janus::Editor
{
namespace
{
std::filesystem::path FromUtf8(std::string_view text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
template <usize N> void Copy(std::array<char, N>& target, std::string_view source)
{
    target.fill(0);
    std::copy_n(source.data(), std::min(source.size(), N - 1), target.data());
}
} // namespace
void ProjectSettingsPanel::Reset(const ProjectSettings& settings)
{
    m_Draft = settings;
    Copy(m_Name, settings.name);
    usize index = 0;
    for (const auto* path : {&settings.defaultScene, &settings.assetRegistry, &settings.assetRoot,
                             &settings.scriptRoot})
    {
        const auto utf8 = path->generic_u8string();
        Copy(m_Paths[index++], std::string(utf8.begin(), utf8.end()));
    }
    m_Initialized = true;
}
ProjectSettings ProjectSettingsPanel::GetDraft() const
{
    auto draft = m_Draft;
    draft.name = m_Name.data();
    draft.defaultScene = FromUtf8(m_Paths[0].data());
    draft.assetRegistry = FromUtf8(m_Paths[1].data());
    draft.assetRoot = FromUtf8(m_Paths[2].data());
    draft.scriptRoot = FromUtf8(m_Paths[3].data());
    return draft;
}
bool ProjectSettingsPanel::HasUnsavedChanges(const ProjectSession& session) const
{
    return m_Initialized && GetDraft() != session.GetProjectSettings();
}
void ProjectSettingsPanel::Draw(ProjectSession& session)
{
    const auto text = [&](const char* key) { return EditorText(m_Language, key); };
    const auto label = [&](const char* key) { return EditorLabel(m_Language, key); };
    if (!m_Initialized)
        Reset(session.GetProjectSettings());
    ImGui::BeginChild("ProjectSettingsForm");
    ImGui::TextWrapped(
        text("Input applies on next Play. Scene and paths apply after reopening the project. "
             "Display timing applies after restarting. Saving settings does not save or change the "
             "Scene."));
    const bool blocked = session.IsAuthoringReadOnly();
    if (blocked)
        ImGui::TextDisabled(text("Stop runtime and finish transaction/recovery to save settings."));
    ImGui::BeginDisabled(blocked);
    ImGui::SetNextItemWidth(320);
    ImGui::InputText(label("Project name").c_str(), m_Name.data(), m_Name.size());
    const char* labels[]{"Default Scene", "Asset registry", "Asset root", "Script root"};
    for (usize i = 0; i < 4; ++i)
    {
        ImGui::SetNextItemWidth(320);
        ImGui::InputText(label(labels[i]).c_str(), m_Paths[i].data(), m_Paths[i].size());
    }
    int resolution[]{static_cast<int>(m_Draft.width), static_cast<int>(m_Draft.height)};
    ImGui::SetNextItemWidth(230);
    if (ImGui::InputInt2(label("Game resolution").c_str(), resolution))
    {
        m_Draft.width = static_cast<u32>(std::max(0, resolution[0]));
        m_Draft.height = static_cast<u32>(std::max(0, resolution[1]));
    }
    ImGui::Checkbox(label("VSync").c_str(), &m_Draft.vsync);
    int fps = static_cast<int>(m_Draft.targetFps);
    ImGui::SetNextItemWidth(160);
    if (ImGui::InputInt(label("Target FPS (0 = uncapped)").c_str(), &fps))
        m_Draft.targetFps = static_cast<u32>(std::max(0, fps));
    ImGui::SeparatorText(text("Input actions"));
    std::optional<std::string> remove;
    for (auto& [name, keys] : m_Draft.inputBindings)
    {
        ImGui::PushID(name.c_str());
        ImGui::TextUnformatted(name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(label("Remove action").c_str()))
            remove = name;
        for (usize i = 0; i < keys.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(140);
            if (ImGui::BeginCombo("##Key", KeyCodeName(keys[i]).data()))
            {
                for (usize k = 0; k < static_cast<usize>(KeyCode::Count); ++k)
                {
                    const auto key = static_cast<KeyCode>(k);
                    if (ImGui::Selectable(KeyCodeName(key).data(), keys[i] == key))
                        keys[i] = key;
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            if (i + 1 < keys.size())
                ImGui::SameLine();
        }
        if (keys.size() < 8 && ImGui::SmallButton(label("Add key").c_str()))
        {
            for (usize k = 0; k < static_cast<usize>(KeyCode::Count); ++k)
            {
                const auto key = static_cast<KeyCode>(k);
                if (std::find(keys.begin(), keys.end(), key) == keys.end())
                {
                    keys.push_back(key);
                    break;
                }
            }
        }
        ImGui::SameLine();
        if (keys.size() > 1 && ImGui::SmallButton(label("Remove last key").c_str()))
            keys.pop_back();
        ImGui::PopID();
    }
    if (remove)
        m_Draft.inputBindings.erase(*remove);
    ImGui::SetNextItemWidth(200);
    ImGui::InputText(label("New action").c_str(), m_NewAction.data(), m_NewAction.size());
    ImGui::SameLine();
    if (ImGui::Button(label("Add action").c_str()) && m_NewAction[0] != 0 &&
        m_Draft.inputBindings.size() < 64)
    {
        m_Draft.inputBindings.try_emplace(m_NewAction.data(), std::vector<KeyCode>{KeyCode::Space});
        m_NewAction.fill(0);
    }
    if (ImGui::Button(label("Save project settings").c_str()))
    {
        const auto result = session.SaveProjectSettings(GetDraft());
        if (result)
            Reset(session.GetProjectSettings());
        m_Message = result ? "Project settings saved." : result.GetError().message;
    }
    ImGui::SameLine();
    if (ImGui::Button(label("Revert settings draft").c_str()))
    {
        Reset(session.GetProjectSettings());
        m_Message.clear();
    }
    ImGui::EndDisabled();
    if (!m_Message.empty())
        ImGui::TextWrapped("%s", m_Message == "Project settings saved."
                                     ? text("Project settings saved.")
                                     : m_Message.c_str());
    ImGui::EndChild();
}
} // namespace Janus::Editor
