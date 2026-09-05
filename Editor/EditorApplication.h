#pragma once

#include "Application/ApplicationClient.h"
#include "Renderer/RendererTypes.h"

#include <filesystem>
#include <memory>
#include <string>

namespace Janus
{

class SceneRenderer;

namespace Editor
{

class AssetBrowserPanel;
class EditorActions;
class EditorCamera;
struct EditorContext;
class HierarchyPanel;
class InspectorPanel;
class ProjectSession;

class EditorApplication final : public ApplicationClient
{
public:
    explicit EditorApplication(std::filesystem::path projectRoot);
    ~EditorApplication() override;

    [[nodiscard]] Result<void> OnInitialize(Application& application) override;
    void OnEvent(const Event& event, Application& application) override;
    void OnUpdate(TimeStep timeStep, Application& application) override;
    void OnShutdown(Application& application) noexcept override;

private:
    void RecordError(const Error& error);
    void ShutdownImGui(Application& application) noexcept;

    std::filesystem::path m_ProjectRoot;
    std::unique_ptr<ProjectSession> m_ProjectSession;
    std::unique_ptr<EditorContext> m_EditorContext;
    std::unique_ptr<EditorActions> m_EditorActions;
    std::unique_ptr<AssetBrowserPanel> m_AssetBrowserPanel;
    std::unique_ptr<HierarchyPanel> m_HierarchyPanel;
    std::unique_ptr<InspectorPanel> m_InspectorPanel;
    std::unique_ptr<EditorCamera> m_EditorCamera;
    std::unique_ptr<SceneRenderer> m_SceneRenderer;

    RenderTargetHandle m_SceneViewTarget;
    RenderTargetHandle m_GameViewTarget;
    Viewport m_SceneViewViewport{640, 360};
    Viewport m_GameViewViewport{640, 360};

    std::string m_LastError;

    bool m_ImGuiContextCreated = false;
    bool m_ImGuiPlatformInitialized = false;
    bool m_ImGuiRendererInitialized = false;
};

} // namespace Editor
} // namespace Janus
