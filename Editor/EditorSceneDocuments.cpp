#include "Core/FileSystem/FileSystem.h"
#include "EditorApplication.h"
#include "EditorCamera.h"
#include "EditorCloseController.h"
#include "EditorContext.h"
#include "Panels/InspectorPanel.h"
#include "ProjectSession.h"
#include <algorithm>
#include <imgui.h>

namespace Janus::Editor
{
void EditorApplication::RefreshSceneDocumentViews()
{
    if (!m_ProjectSession || m_ViewSceneRevision == m_ProjectSession->GetSceneRevision())
        return;
    m_ViewSceneRevision = m_ProjectSession->GetSceneRevision();
    m_TransformDrag.Cancel();
    m_EditorContext->selection.Clear();
    m_InspectorPanel->DiscardPendingEdit();
    *m_EditorCamera = EditorCamera{};
    m_InitialFrame = true;
    m_SelectSceneViewTab = true;
    m_GameInputActive = false;
    m_GameInput = {};
    m_SuppressGameUntilReleased = true;
}

void EditorApplication::DrawSceneDocumentDialog()
{
    const auto label = [&](const char* key) { return EditorLabel(m_Preferences.language, key); };
    const auto text = [&](const char* key) { return EditorText(m_Preferences.language, key); };
    const auto popup = label("Scene document");
    if (m_OpenSceneDialog)
    {
        ImGui::OpenPopup(popup.c_str());
        m_OpenSceneDialog = false;
    }
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::min(580.0f * m_UiScale, viewport->WorkSize.x - 24), 0},
                             ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(popup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    const char* titles[]{"", "New Scene...", "Open Scene...", "Save Scene As..."};
    ImGui::TextUnformatted(text(titles[m_SceneDocumentAction]));
    ImGui::TextWrapped("%s: %s", text("Current scene"),
                       FileSystem::PathToUtf8(m_ProjectSession->GetCurrentScenePath()).c_str());
    auto finish = [&]
    {
        if (m_SceneLeave)
            m_SceneLeave->Cancel();
        m_SceneLeave.reset();
        m_PreparedScene.reset();
        m_SceneDocumentAction = 0;
        m_SuppressGameUntilReleased = true;
        ImGui::CloseCurrentPopup();
    };
    if (m_CloseRequested)
    {
        finish();
        ImGui::EndPopup();
        return;
    }
    if (!m_PreparedScene)
    {
        ImGui::TextWrapped("%s",
                           text("Use a project-relative .scene path in an existing directory."));
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##ScenePath", "Scenes/Example.scene", m_SceneDocumentPath.data(),
                                 m_SceneDocumentPath.size());
        if (m_SceneDocumentAction == 3)
            ImGui::Checkbox(label("Overwrite existing target").c_str(), &m_SceneOverwrite);
        if (ImGui::Button(
                label(m_SceneDocumentAction == 3 ? "Save Scene As..." : "Prepare scene").c_str()))
        {
            auto settled = m_InspectorPanel->CommitPendingEdit();
            if (!settled)
                m_SceneDocumentError = settled.GetError().message;
            else
            {
                const auto path = std::filesystem::path(
                    std::u8string(reinterpret_cast<const char8_t*>(m_SceneDocumentPath.data())));
                if (m_SceneDocumentAction == 3)
                {
                    auto saved = m_ProjectSession->SaveSceneAs(path, m_SceneOverwrite);
                    m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human,
                                                                      "scene.save_as", saved);
                    if (saved)
                        finish();
                    else
                        m_SceneDocumentError = saved.GetError().message;
                }
                else
                {
                    auto prepared = m_SceneDocumentAction == 1
                                        ? m_ProjectSession->PrepareNewScene(path)
                                        : m_ProjectSession->PrepareOpenScene(path);
                    if (!prepared)
                        m_SceneDocumentError = prepared.GetError().message;
                    else
                    {
                        m_PreparedScene = std::move(prepared).Value();
                        m_SceneLeave = std::make_unique<EditorCloseController>(*m_ProjectSession);
                        m_SceneLeave->Request();
                        m_SceneDocumentError.clear();
                    }
                }
            }
        }
        if (!m_SceneDocumentError.empty() && ImGui::Button(label("Discard field draft").c_str()))
            m_InspectorPanel->DiscardPendingEdit();
    }
    else
    {
        ImGui::TextWrapped("%s: %s", text("Prepared scene"),
                           FileSystem::PathToUtf8(m_PreparedScene->GetPath()).c_str());
        ImGui::TextWrapped("%s", text(m_ProjectSession->IsDirty() ? "Scene has unsaved changes."
                                                                  : "Scene is saved."));
        if (m_ProjectSession->HasRuntime())
            ImGui::Checkbox(label("Stop runtime before changing scene").c_str(),
                            &m_SceneStopRuntime);
        auto& commands = m_ProjectSession->GetCommandBus();
        if (commands.HasTransaction() && !commands.RecoveryRequired())
        {
            ImGui::TextWrapped(
                "%s", text("Finish or roll back the Agent transaction, then prepare again."));
            if (ImGui::Button(label("Roll back Agent transaction").c_str()))
            {
                auto rolled = m_SceneLeave->RollbackTransaction();
                if (!rolled)
                    m_SceneDocumentError = rolled.GetError().message;
                else
                {
                    m_SceneLeave->Cancel();
                    m_SceneLeave.reset();
                    m_PreparedScene.reset();
                }
            }
        }
        if (m_PreparedScene)
        {
            auto confirm = [&](bool save)
            {
                auto changed =
                    m_SceneLeave->ConfirmSceneChange(*m_PreparedScene, save, m_SceneStopRuntime);
                m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human,
                                                                  "scene.change", changed);
                if (changed)
                    finish();
                else
                    m_SceneDocumentError = changed.GetError().message;
            };
            const bool ready = (!commands.HasTransaction() || commands.RecoveryRequired()) &&
                               (!m_ProjectSession->HasRuntime() || m_SceneStopRuntime);
            ImGui::BeginDisabled(!ready || commands.RecoveryRequired());
            if (ImGui::Button(label("Save and change scene").c_str()))
                confirm(true);
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!ready || !m_PreparedScene);
            if (ImGui::Button(label("Discard changes and change scene").c_str()))
                confirm(false);
            ImGui::EndDisabled();
        }
    }
    if (!m_SceneDocumentError.empty())
        ImGui::TextWrapped("%s", m_SceneDocumentError.c_str());
    if (ImGui::Button(label("Cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        finish();
    ImGui::EndPopup();
}
} // namespace Janus::Editor
