#include "EditorApplication.h"
#include "EditorCamera.h"
#include "EditorConsole.h"
#include "EditorContext.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ProjectSettingsPanel.h"
#include "Platform/UserDirectories.h"
#include "ProjectSession.h"

#include <SDL3/SDL_stdinc.h>
#include <imgui.h>

namespace Janus::Editor
{
void EditorApplication::LoadPreferences()
{
    // Explicit override is for isolated local runs. Default storage never uses the project.
    if (const char* overridePath = SDL_getenv("JANUS_EDITOR_PREFERENCES_PATH");
        overridePath && *overridePath)
    {
        const std::string text(overridePath);
        m_PreferencesPath = std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    else
    {
        const auto directory = Platform::GetUserDataDirectory();
        if (!directory)
        {
            m_PreferencesError = directory.GetError().message;
            return;
        }
        m_PreferencesPath = directory.Value() / "editor-preferences.json";
    }
    const auto loaded = EditorPreferences::Load(m_PreferencesPath);
    if (loaded)
    {
        m_Preferences = loaded.Value();
        if (m_Preferences.wasClamped)
            m_PreferencesError =
                "Some saved preferences were outside supported bounds and were clamped.";
    }
    else
        m_PreferencesError =
            "Preferences could not be loaded; using defaults. " + loaded.GetError().message;
    m_UserScale = m_Preferences.userScale;
    m_WorkspacePreferences = m_Preferences.workspace;
    m_ShowGrid = m_Preferences.showGrid;
    const auto snapshot = m_Preferences.Serialize();
    if (snapshot)
        m_PreferencesSnapshot = snapshot.Value();
}

void EditorApplication::SetEditorLanguage(EditorLanguage language)
{
    if (language == EditorLanguage::Chinese && !m_ChineseFontAvailable)
    {
        m_PreferencesError =
            "Chinese font unavailable. Install a system Chinese font to enable Chinese labels.";
        return;
    }
    m_Preferences.language = language;
    if (m_EditorContext)
        m_EditorContext->language = language;
    if (m_ConsolePanel)
        m_ConsolePanel->SetLanguage(language);
    if (m_ProjectSettingsPanel)
        m_ProjectSettingsPanel->SetLanguage(language);
}

void EditorApplication::UpdatePreferences(bool flush)
{
    if (m_PreferencesPath.empty() || !m_ProjectSession || !m_EditorCamera)
        return;
    m_Preferences.userScale = m_UserScale;
    m_Preferences.workspace = m_WorkspacePreferences;
    m_Preferences.showGrid = m_ShowGrid;
    if (m_WorkspaceLogicalSize.x > 0 && m_WorkspaceLogicalSize.y > 0)
        m_Preferences.workspaceReferenceSize = m_WorkspaceLogicalSize;
    if (!m_InitialFrame && !m_PreferencesProjectKey.empty())
    {
        const auto remembered = m_Preferences.RememberCamera(
            {m_PreferencesProjectKey, m_ProjectSession->GetCurrentScenePath(),
             m_EditorCamera->GetPosition(), m_EditorCamera->GetZoom()});
        if (!remembered)
        {
            m_PreferencesError = remembered.GetError().message;
            return;
        }
    }
    const auto snapshot = m_Preferences.Serialize();
    if (!snapshot)
    {
        m_PreferencesError = snapshot.GetError().message;
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (snapshot.Value() != m_PreferencesSnapshot)
    {
        m_PreferencesSnapshot = snapshot.Value();
        m_PreferencesChanged = now;
        m_PreferencesPending = true;
        m_PreferencesSaveFailed = false;
    }
    const bool activeInput = m_ImGuiContextCreated && ImGui::IsAnyItemActive();
    if (!m_PreferencesPending ||
        (!flush && (m_PreferencesSaveFailed || activeInput ||
                    now - m_PreferencesChanged < std::chrono::milliseconds(500))))
        return;
    const auto saved = m_Preferences.Save(m_PreferencesPath);
    if (saved)
    {
        m_PreferencesPending = false;
        m_PreferencesSaveFailed = false;
    }
    else
    {
        m_PreferencesError = "Preferences could not be saved. " + saved.GetError().message;
        if (!m_PreferencesSaveFailed && m_EditorConsole)
            m_EditorConsole->PushError({ErrorCode::InvalidState, m_PreferencesError});
        m_PreferencesSaveFailed = true;
    }
}
} // namespace Janus::Editor
