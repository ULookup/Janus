#pragma once

#include "Application/ApplicationClient.h"

#include <filesystem>
#include <memory>

namespace Janus::Editor
{

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
    void ShutdownImGui(Application& application) noexcept;

    std::filesystem::path m_ProjectRoot;
    std::unique_ptr<ProjectSession> m_ProjectSession;
    bool m_ImGuiContextCreated = false;
    bool m_ImGuiPlatformInitialized = false;
    bool m_ImGuiRendererInitialized = false;
};

} // namespace Janus::Editor
