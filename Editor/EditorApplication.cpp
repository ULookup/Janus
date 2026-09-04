#include "EditorApplication.h"

#include "ProjectSession.h"

#include "Application/Application.h"
#include "Core/Input/InputState.h"
#include "Core/Log/Log.h"
#include "Renderer/OrthographicCamera.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <string>
#include <utility>

namespace Janus::Editor
{

EditorApplication::EditorApplication(std::filesystem::path projectRoot)
    : m_ProjectRoot(std::move(projectRoot))
{
}

Result<void> EditorApplication::OnInitialize(Application& application)
{
    auto& window = application.GetWindow();
    auto* nativeWindow = static_cast<SDL_Window*>(window.GetNativeHandle());
    if (nativeWindow == nullptr)
    {
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "JanusEditor requires an SDL-backed native window.");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_ImGuiContextCreated = true;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForOpenGL(nativeWindow, nullptr))
    {
        ShutdownImGui(application);
        return Result<void>::Failure(
            ErrorCode::InvalidState,
            "Failed to initialize Dear ImGui SDL3 backend.");
    }
    m_ImGuiPlatformInitialized = true;

    if (!ImGui_ImplOpenGL3_Init("#version 450"))
    {
        ShutdownImGui(application);
        return Result<void>::Failure(
            ErrorCode::RendererInitFailed,
            "Failed to initialize Dear ImGui OpenGL3 backend.");
    }
    m_ImGuiRendererInitialized = true;

    window.SetNativeEventCallback(
        [](const void* nativeEvent)
        {
            ImGui_ImplSDL3_ProcessEvent(
                static_cast<const SDL_Event*>(nativeEvent));
        });

    ProjectRuntimeConfig project;
    project.root = m_ProjectRoot;

    auto session = ProjectSession::Open(
        project,
        application.GetRenderer2D());
    if (!session)
    {
        const Error error = session.GetError();
        ShutdownImGui(application);
        return Result<void>::Failure(error);
    }

    m_ProjectSession = std::move(session).Value();

    JANUS_INFO(
        "JanusEditor opened project '{}' with Scene '{}'.",
        m_ProjectRoot.string(),
        m_ProjectSession->GetEditorScene().GetMetadata().name);

    return Result<void>::Success();
}

void EditorApplication::OnEvent(const Event&, Application&)
{
}

void EditorApplication::OnUpdate(
    TimeStep,
    Application& application)
{
    auto& window = application.GetWindow();
    auto& renderer = application.GetRenderer2D();

    const Viewport viewport{
        window.GetWidth(),
        window.GetHeight()};

    renderer.SetViewport(viewport);
    renderer.BeginFrame(OrthographicCamera{});
    const auto clearResult = renderer.EndFrame();
    if (!clearResult)
    {
        JANUS_ERROR(
            "Editor frame clear failed: {}",
            clearResult.GetError().message);
        application.RequestExit();
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Janus Editor");

    if (m_ProjectSession != nullptr)
    {
        const auto& scene = m_ProjectSession->GetEditorScene();
        const std::string projectRoot =
            m_ProjectSession->GetProjectRoot().string();
        const std::string scenePath =
            m_ProjectSession->GetCurrentScenePath().generic_string();

        ImGui::TextUnformatted("v0.6 Editor Foundation");
        ImGui::Separator();
        ImGui::Text("Project: %s", projectRoot.c_str());
        ImGui::Text("Scene: %s", scene.GetMetadata().name.c_str());
        ImGui::Text("Scene file: %s", scenePath.c_str());
        ImGui::Text(
            "Entities: %llu",
            static_cast<unsigned long long>(scene.GetEntities().size()));
        ImGui::Text(
            "Registered assets: %llu",
            static_cast<unsigned long long>(
                m_ProjectSession->GetAssetRegistry().Size()));
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (application.GetInput().WasKeyPressed(KeyCode::Escape))
    {
        application.RequestExit();
    }
}

void EditorApplication::OnShutdown(Application& application) noexcept
{
    // AssetService owns renderer-backed resources, so the project session must
    // disappear while Application still owns a live Renderer2D.
    m_ProjectSession.reset();
    ShutdownImGui(application);
}

void EditorApplication::ShutdownImGui(Application& application) noexcept
{
    application.GetWindow().SetNativeEventCallback({});

    if (m_ImGuiRendererInitialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        m_ImGuiRendererInitialized = false;
    }

    if (m_ImGuiPlatformInitialized)
    {
        ImGui_ImplSDL3_Shutdown();
        m_ImGuiPlatformInitialized = false;
    }

    if (m_ImGuiContextCreated)
    {
        ImGui::DestroyContext();
        m_ImGuiContextCreated = false;
    }
}

} // namespace Janus::Editor
