#include "EditorApplication.h"

#include "EditorActions.h"
#include "EditorCamera.h"
#include "EditorContext.h"
#include "EditorConsole.h"
#include "EditorViewSource.h"
#include "EditorWorkspaceLayout.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "ProjectSession.h"
#include "ScenePicker.h"

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
#include <optional>
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



void ApplyWorkspaceRect(
    const EditorPanelRect& rect,
    const ImGuiViewport& viewport)
{
    ImGui::SetNextWindowPos(
        ImVec2{
            viewport.WorkPos.x + rect.x,
            viewport.WorkPos.y + rect.y},
        ImGuiCond_Always);

    ImGui::SetNextWindowSize(
        ImVec2{
            std::max(rect.width, 1.0f),
            std::max(rect.height, 1.0f)},
        ImGuiCond_Always);
}

constexpr ImGuiWindowFlags WorkspacePanelFlags =
    ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoCollapse;

constexpr ImGuiWindowFlags ToolbarFlags =
    ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoCollapse
    | ImGuiWindowFlags_NoScrollbar
    | ImGuiWindowFlags_NoSavedSettings;

void DrawSceneGrid(
    const EditorCamera& camera,
    Viewport viewport,
    ImVec2 rectMin,
    ImVec2 rectMax)
{
    if (viewport.width == 0 || viewport.height == 0)
    {
        return;
    }

    const f32 zoom = camera.GetZoom();
    if (zoom <= 0.0f)
    {
        return;
    }

    f32 worldSpacing = 64.0f;
    f32 pixelSpacing = worldSpacing / zoom;

    while (pixelSpacing < 24.0f)
    {
        worldSpacing *= 2.0f;
        pixelSpacing = worldSpacing / zoom;
    }

    while (pixelSpacing > 160.0f && worldSpacing > 1.0f)
    {
        worldSpacing *= 0.5f;
        pixelSpacing = worldSpacing / zoom;
    }

    const Vector2 worldTopLeft =
        camera.ScreenToWorld(
            Vector2{0.0f, 0.0f},
            viewport);
    const Vector2 worldBottomRight =
        camera.ScreenToWorld(
            Vector2{
                static_cast<f32>(viewport.width),
                static_cast<f32>(viewport.height)},
            viewport);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(rectMin, rectMax, true);

    const ImU32 gridColor =
        ImGui::GetColorU32(
            ImGuiCol_Border,
            0.35f);
    const ImU32 axisColor =
        ImGui::GetColorU32(
            ImGuiCol_TextDisabled,
            0.65f);

    const f32 firstX =
        std::floor(worldTopLeft.x / worldSpacing)
        * worldSpacing;

    for (f32 worldX = firstX;
         worldX <= worldBottomRight.x;
         worldX += worldSpacing)
    {
        const f32 screenX =
            rectMin.x
            + (worldX - worldTopLeft.x) / zoom;

        drawList->AddLine(
            ImVec2{screenX, rectMin.y},
            ImVec2{screenX, rectMax.y},
            std::abs(worldX) < 0.001f
                ? axisColor
                : gridColor);
    }

    const f32 firstY =
        std::floor(worldBottomRight.y / worldSpacing)
        * worldSpacing;

    for (f32 worldY = firstY;
         worldY <= worldTopLeft.y;
         worldY += worldSpacing)
    {
        const f32 screenY =
            rectMin.y
            + (worldTopLeft.y - worldY) / zoom;

        drawList->AddLine(
            ImVec2{rectMin.x, screenY},
            ImVec2{rectMax.x, screenY},
            std::abs(worldY) < 0.001f
                ? axisColor
                : gridColor);
    }

    drawList->PopClipRect();
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

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowPadding = ImVec2{8.0f, 8.0f};

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

    m_EditorContext = std::make_unique<EditorContext>();
    m_EditorContext->project = m_ProjectSession.get();
    m_EditorActions =
        std::make_unique<EditorActions>(*m_EditorContext);
    m_EditorConsole =
        std::make_unique<EditorConsole>();
    m_ConsolePanel =
        std::make_unique<ConsolePanel>(*m_EditorConsole);
    m_AssetBrowserPanel =
        std::make_unique<AssetBrowserPanel>(
            *m_EditorContext,
            *m_EditorActions);
    m_HierarchyPanel =
        std::make_unique<HierarchyPanel>(
            *m_EditorContext,
            *m_EditorActions);
    m_InspectorPanel =
        std::make_unique<InspectorPanel>(
            *m_EditorContext,
            *m_EditorActions);

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
        m_InspectorPanel.reset();
        m_HierarchyPanel.reset();
        m_AssetBrowserPanel.reset();
        m_ConsolePanel.reset();
        m_EditorConsole.reset();
        m_EditorActions.reset();
        m_EditorContext.reset();
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
        m_InspectorPanel.reset();
        m_HierarchyPanel.reset();
        m_AssetBrowserPanel.reset();
        m_ConsolePanel.reset();
        m_EditorConsole.reset();
        m_EditorActions.reset();
        m_EditorContext.reset();
        m_ProjectSession.reset();
        ShutdownImGui(application);
        return Result<void>::Failure(error);
    }

    m_GameViewTarget = gameTarget.Value();

    m_EditorConsole->PushInfo(
        "Opened project '" + m_ProjectRoot.string()
        + "' with Scene '"
        + m_ProjectSession->GetEditorScene().GetMetadata().name
        + "'.");

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

    const ImGuiViewport* mainViewport =
        ImGui::GetMainViewport();
    const EditorWorkspaceLayout workspace =
        BuildEditorWorkspaceLayout(
            mainViewport->WorkSize.x,
            mainViewport->WorkSize.y);

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
        ApplyWorkspaceRect(
            workspace.toolbar,
            *mainViewport);

        ImGui::Begin(
            "##JanusToolbar",
            nullptr,
            ToolbarFlags);

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
                    if (m_EditorConsole != nullptr)
                    {
                        m_EditorConsole->PushInfo(
                            "Play Mode started.");
                    }
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
            else if (m_EditorConsole != nullptr)
            {
                m_EditorConsole->PushInfo(
                    "Play Mode stopped.");
            }
        }

        ImGui::SameLine();

        const bool canSave =
            !m_ProjectSession->IsPlaying()
            && m_ProjectSession->IsDirty();

        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button("Save"))
        {
            const auto saved =
                m_ProjectSession->SaveCurrentScene();
            if (!saved)
            {
                RecordError(saved.GetError());
            }
            else
            {
                m_LastError.clear();
                if (m_EditorConsole != nullptr)
                {
                    m_EditorConsole->PushInfo(
                        "Scene saved.");
                }
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextUnformatted(
            m_ProjectSession->IsPlaying()
                ? "Playing"
                : "Editing");

        if (m_ProjectSession->IsDirty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("* Unsaved");
        }

        const auto& scene =
            m_ProjectSession->GetEditorScene();

        ImGui::SameLine(
            0.0f,
            24.0f);
        ImGui::Separator();
        ImGui::SameLine();

        ImGui::Text(
            "Scene: %s   Entities: %llu   Assets: %llu",
            scene.GetMetadata().name.c_str(),
            static_cast<unsigned long long>(
                scene.GetEntities().size()),
            static_cast<unsigned long long>(
                m_ProjectSession->GetAssetRegistry().Size()));

        if (ImGui::IsItemHovered())
        {
            const std::string projectRoot =
                m_ProjectSession->GetProjectRoot().string();
            const std::string scenePath =
                m_ProjectSession->GetCurrentScenePath().generic_string();

            ImGui::SetTooltip(
                "Project: %s\nScene file: %s",
                projectRoot.c_str(),
                scenePath.c_str());
        }

        if (!m_LastError.empty())
        {
            ImGui::SameLine(
                0.0f,
                24.0f);
            ImGui::TextDisabled(
                "Last error: %s",
                m_LastError.c_str());
        }

        ImGui::End();
    }

    if (m_EditorContext != nullptr
        && m_HierarchyPanel != nullptr
        && m_InspectorPanel != nullptr)
    {
        if (m_ProjectSession != nullptr)
        {
            m_EditorContext->selection.Validate(
                m_ProjectSession->GetEditorScene());
        }

        ApplyWorkspaceRect(
            workspace.hierarchy,
            *mainViewport);
        const auto hierarchyError =
            m_HierarchyPanel->Draw();
        if (hierarchyError.has_value())
        {
            RecordError(*hierarchyError);
        }

        ApplyWorkspaceRect(
            workspace.inspector,
            *mainViewport);
        const auto inspectorError =
            m_InspectorPanel->Draw();
        if (inspectorError.has_value())
        {
            RecordError(*inspectorError);
        }

        if (m_AssetBrowserPanel != nullptr)
        {
            ApplyWorkspaceRect(
                workspace.assetBrowser,
                *mainViewport);
            const auto assetError =
                m_AssetBrowserPanel->Draw();
            if (assetError.has_value())
            {
                RecordError(*assetError);
            }
        }

        if (m_ConsolePanel != nullptr)
        {
            ApplyWorkspaceRect(
                workspace.console,
                *mainViewport);
            m_ConsolePanel->Draw();
        }
    }

    bool renderSceneView = false;
    bool renderGameView = false;
    std::optional<Vector2> pendingScenePick;

    if (m_ProjectSession != nullptr
        && m_EditorCamera != nullptr
        && m_SceneRenderer != nullptr)
    {
        ApplyWorkspaceRect(
            workspace.sceneView,
            *mainViewport);
        const bool sceneViewVisible =
            ImGui::Begin(
                "Scene View",
                nullptr,
                WorkspacePanelFlags);

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

                    DrawSceneGrid(
                        *m_EditorCamera,
                        m_SceneViewViewport,
                        ImGui::GetItemRectMin(),
                        ImGui::GetItemRectMax());

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

                        if (ImGui::IsMouseClicked(
                                ImGuiMouseButton_Left))
                        {
                            const ImVec2 itemMin =
                                ImGui::GetItemRectMin();
                            const ImVec2 mouse =
                                ImGui::GetMousePos();

                            const f32 localX =
                                mouse.x - itemMin.x;
                            const f32 localY =
                                mouse.y - itemMin.y;

                            const f32 scaleX =
                                static_cast<f32>(
                                    m_SceneViewViewport.width)
                                / available.x;
                            const f32 scaleY =
                                static_cast<f32>(
                                    m_SceneViewViewport.height)
                                / available.y;

                            pendingScenePick =
                                Vector2{
                                    localX * scaleX,
                                    localY * scaleY};
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

        ApplyWorkspaceRect(
            workspace.gameView,
            *mainViewport);
        const bool gameViewVisible =
            ImGui::Begin(
                "Game View",
                nullptr,
                WorkspacePanelFlags);

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
        else if (pendingScenePick.has_value()
            && m_EditorContext != nullptr)
        {
            const auto picked =
                PickSpriteEntity(
                    scene,
                    *m_EditorCamera,
                    m_SceneViewViewport,
                    *pendingScenePick);

            if (picked.has_value())
            {
                m_EditorContext->selection.Select(*picked);
            }
            else
            {
                m_EditorContext->selection.Clear();
            }
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

    m_InspectorPanel.reset();
    m_HierarchyPanel.reset();
    m_AssetBrowserPanel.reset();
    m_ConsolePanel.reset();
    m_EditorConsole.reset();
    m_EditorActions.reset();
    m_EditorContext.reset();

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

    if (m_EditorConsole != nullptr)
    {
        m_EditorConsole->PushError(error);
    }

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
