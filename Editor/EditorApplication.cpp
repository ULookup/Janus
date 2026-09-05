#include "EditorApplication.h"

#include "EditorCamera.h"
#include "EditorViewSource.h"
#include "ProjectSession.h"

#include "Application/Application.h"
#include "Core/Input/InputState.h"
#include "Core/Log/Log.h"
#include "Platform/Window/Window.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace Janus::Editor
{

namespace
{

constexpr u32 InitialViewWidth = 640;
constexpr u32 InitialViewHeight = 360;

[[nodiscard]] bool HasUsableContentSize(
    const ImVec2& size) noexcept
{
    return size.x >= 1.0f && size.y >= 1.0f;
}

[[nodiscard]] Viewport ToViewport(
    const ImVec2& size) noexcept
{
    return Viewport{
        static_cast<u32>(
            std::max(1.0f, std::floor(size.x))),
        static_cast<u32>(
            std::max(1.0f, std::floor(size.y)))};
}

[[nodiscard]] ImTextureRef ToImGuiTexture(
    TexturePresentationHandle handle)
{
    return ImTextureRef{
        static_cast<ImTextureID>(handle.value)};
}

} // namespace

EditorApplication::EditorApplication(std::filesystem::path projectRoot)
    : m_ProjectRoot(std::move(projectRoot))
{
}

EditorApplication::~EditorApplication() = default;

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
    m_EditorCamera = std::make_unique<EditorCamera>();
    m_SceneRenderer = std::make_unique<SceneRenderer>();

    auto& renderer = application.GetRenderer2D();

    auto sceneTarget = renderer.CreateRenderTarget(
        RenderTargetDesc{
            InitialViewWidth,
            InitialViewHeight});
    if (!sceneTarget)
    {
        const Error error = sceneTarget.GetError();
        m_SceneRenderer.reset();
        m_EditorCamera.reset();
        m_ProjectSession.reset();
        ShutdownImGui(application);
        return Result<void>::Failure(error);
    }

    m_SceneViewTarget = sceneTarget.Value();

    auto gameTarget = renderer.CreateRenderTarget(
        RenderTargetDesc{
            InitialViewWidth,
            InitialViewHeight});
    if (!gameTarget)
    {
        const Error error = gameTarget.GetError();
        const auto destroyed =
            renderer.DestroyRenderTarget(m_SceneViewTarget);
        if (!destroyed)
        {
            JANUS_ERROR(
                "Failed to clean Scene View target after initialization "
                "failure: {}",
                destroyed.GetError().message);
        }

        m_SceneViewTarget = {};
        m_SceneRenderer.reset();
        m_EditorCamera.reset();
        m_ProjectSession.reset();
        ShutdownImGui(application);
        return Result<void>::Failure(error);
    }

    m_GameViewTarget = gameTarget.Value();

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
    TimeStep timeStep,
    Application& application)
{
    auto& window = application.GetWindow();
    auto& renderer = application.GetRenderer2D();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (m_ProjectSession == nullptr
        || m_EditorCamera == nullptr
        || m_SceneRenderer == nullptr)
    {
        RecordError(
            Error{
                ErrorCode::InvalidState,
                "Editor session state is incomplete."});
    }

    if (m_ProjectSession != nullptr)
    {
        ImGui::Begin(
            "Play Controls",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize);

        if (!m_ProjectSession->IsPlaying())
        {
            if (ImGui::Button("Play"))
            {
                const auto started =
                    m_ProjectSession->StartRuntime(
                        application.GetInput());
                if (!started)
                {
                    RecordError(started.GetError());
                }
                else
                {
                    m_LastError.clear();
                }
            }
        }
        else if (ImGui::Button("Stop"))
        {
            const auto stopped =
                m_ProjectSession->StopRuntime();
            if (!stopped)
            {
                RecordError(stopped.GetError());
            }
        }

        ImGui::SameLine();
        ImGui::TextUnformatted(
            m_ProjectSession->IsPlaying()
                ? "Playing"
                : "Editing");

        ImGui::End();
    }

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
            static_cast<unsigned long long>(
                scene.GetEntities().size()));
        ImGui::Text(
            "Registered assets: %llu",
            static_cast<unsigned long long>(
                m_ProjectSession->GetAssetRegistry().Size()));
    }

    if (!m_LastError.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped(
            "Last error: %s",
            m_LastError.c_str());
    }

    ImGui::End();

    bool renderSceneView = false;
    bool renderGameView = false;

    if (m_ProjectSession != nullptr
        && m_EditorCamera != nullptr
        && m_SceneRenderer != nullptr)
    {
        const bool sceneViewVisible =
            ImGui::Begin("Scene View");

        if (sceneViewVisible)
        {
            const ImVec2 available =
                ImGui::GetContentRegionAvail();

            if (HasUsableContentSize(available))
            {
                const Viewport requested =
                    ToViewport(available);

                if (requested.width != m_SceneViewViewport.width
                    || requested.height != m_SceneViewViewport.height)
                {
                    const auto resized =
                        renderer.ResizeRenderTarget(
                            m_SceneViewTarget,
                            requested.width,
                            requested.height);
                    if (resized)
                    {
                        m_SceneViewViewport = requested;
                    }
                    else
                    {
                        RecordError(resized.GetError());
                    }
                }

                const auto presentation =
                    renderer.GetRenderTargetPresentationHandle(
                        m_SceneViewTarget);

                if (presentation)
                {
                    ImGui::Image(
                        ToImGuiTexture(presentation.Value()),
                        available,
                        ImVec2{0.0f, 1.0f},
                        ImVec2{1.0f, 0.0f});

                    const bool hovered =
                        ImGui::IsItemHovered();
                    if (hovered)
                    {
                        const ImGuiIO& io = ImGui::GetIO();

                        if (io.MouseWheel != 0.0f)
                        {
                            m_EditorCamera->Zoom(io.MouseWheel);
                        }

                        if (ImGui::IsMouseDragging(
                                ImGuiMouseButton_Middle))
                        {
                            m_EditorCamera->PanPixels(
                                Vector2{
                                    io.MouseDelta.x,
                                    io.MouseDelta.y});
                        }
                    }

                    renderSceneView = true;
                }
                else
                {
                    RecordError(presentation.GetError());
                }
            }
        }

        ImGui::End();

        const bool gameViewVisible =
            ImGui::Begin("Game View");

        if (gameViewVisible)
        {
            const ImVec2 available =
                ImGui::GetContentRegionAvail();

            if (HasUsableContentSize(available))
            {
                const Viewport requested =
                    ToViewport(available);

                if (requested.width != m_GameViewViewport.width
                    || requested.height != m_GameViewViewport.height)
                {
                    const auto resized =
                        renderer.ResizeRenderTarget(
                            m_GameViewTarget,
                            requested.width,
                            requested.height);
                    if (resized)
                    {
                        m_GameViewViewport = requested;
                    }
                    else
                    {
                        RecordError(resized.GetError());
                    }
                }

                const auto presentation =
                    renderer.GetRenderTargetPresentationHandle(
                        m_GameViewTarget);

                if (presentation)
                {
                    ImGui::Image(
                        ToImGuiTexture(presentation.Value()),
                        available,
                        ImVec2{0.0f, 1.0f},
                        ImVec2{1.0f, 0.0f});
                    renderGameView = true;
                }
                else
                {
                    RecordError(presentation.GetError());
                }
            }
        }

        ImGui::End();
    }

    if (m_ProjectSession != nullptr
        && m_ProjectSession->IsPlaying())
    {
        const auto updated =
            m_ProjectSession->UpdateRuntime(timeStep);
        if (!updated)
        {
            RecordError(updated.GetError());

            const auto stopped =
                m_ProjectSession->StopRuntime();
            if (!stopped)
            {
                RecordError(stopped.GetError());
            }
        }
    }

    if (renderSceneView)
    {
        Scene& scene =
            ResolveSceneViewScene(*m_ProjectSession);

        const auto rendered =
            m_SceneRenderer->Render(
                SceneRenderRequest{
                    scene,
                    m_ProjectSession->GetAssetService(),
                    renderer,
                    m_EditorCamera->ToRenderCamera(),
                    m_SceneViewViewport,
                    m_SceneViewTarget});

        if (!rendered)
        {
            RecordError(rendered.GetError());
        }
    }

    if (renderGameView)
    {
        Scene& scene =
            ResolveGameViewScene(*m_ProjectSession);

        const auto camera =
            m_SceneRenderer->ResolvePrimaryCamera(scene);
        if (!camera)
        {
            RecordError(camera.GetError());
        }
        else
        {
            const auto rendered =
                m_SceneRenderer->Render(
                    SceneRenderRequest{
                        scene,
                        m_ProjectSession->GetAssetService(),
                        renderer,
                        camera.Value(),
                        m_GameViewViewport,
                        m_GameViewTarget});

            if (!rendered)
            {
                RecordError(rendered.GetError());
            }
        }
    }

    const Viewport windowViewport{
        window.GetWidth(),
        window.GetHeight()};

    if (windowViewport.width != 0
        && windowViewport.height != 0)
    {
        RenderFrameDesc editorBackground;
        editorBackground.viewport = windowViewport;
        editorBackground.clearColor =
            Color{0.08f, 0.08f, 0.09f, 1.0f};

        const auto began =
            renderer.BeginFrame(editorBackground);
        if (!began)
        {
            RecordError(began.GetError());
        }
        else
        {
            const auto cleared = renderer.EndFrame();
            if (!cleared)
            {
                RecordError(cleared.GetError());
            }
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (application.GetInput().WasKeyPressed(KeyCode::Escape))
    {
        application.RequestExit();
    }
}

void EditorApplication::OnShutdown(Application& application) noexcept
{
    auto& renderer = application.GetRenderer2D();

    if (m_ProjectSession != nullptr
        && m_ProjectSession->IsPlaying())
    {
        const auto stopped =
            m_ProjectSession->StopRuntime();
        if (!stopped)
        {
            JANUS_ERROR(
                "Editor runtime shutdown failed: {}",
                stopped.GetError().message);
        }
    }

    if (m_SceneViewTarget.value != 0)
    {
        const auto destroyed =
            renderer.DestroyRenderTarget(m_SceneViewTarget);
        if (!destroyed)
        {
            JANUS_ERROR(
                "Scene View target shutdown failed: {}",
                destroyed.GetError().message);
        }
        m_SceneViewTarget = {};
    }

    if (m_GameViewTarget.value != 0)
    {
        const auto destroyed =
            renderer.DestroyRenderTarget(m_GameViewTarget);
        if (!destroyed)
        {
            JANUS_ERROR(
                "Game View target shutdown failed: {}",
                destroyed.GetError().message);
        }
        m_GameViewTarget = {};
    }

    // AssetService owns renderer-backed resources, so the project session must
    // disappear while Application still owns a live Renderer2D.
    m_ProjectSession.reset();
    m_SceneRenderer.reset();
    m_EditorCamera.reset();

    ShutdownImGui(application);
}

void EditorApplication::RecordError(const Error& error)
{
    m_LastError = error.message;
    JANUS_ERROR(
        "JanusEditor operation failed: {}",
        error.message);
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
