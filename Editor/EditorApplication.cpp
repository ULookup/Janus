#include "EditorApplication.h"
#include "Panels/ProjectSettingsPanel.h"

#include "EditorActions.h"
#include "EditorCamera.h"
#include "EditorCloseController.h"
#include "EditorConsole.h"
#include "EditorContext.h"
#include "EditorGameInput.h"
#include "EditorIcons.h"
#include "EditorViewSource.h"
#include "EditorWorkspaceLayout.h"
#include "McpEditorHost.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "ProjectSession.h"
#include "ScenePicker.h"

#include "Application/Application.h"
#include "Core/FileSystem/FileSystem.h"
#include "Core/Input/InputState.h"
#include "Core/Log/Log.h"
#include "Host/McpPermissionPolicy.h"
#include "Platform/UserDirectories.h"
#include "Platform/Window/Window.h"
#include "Renderer/Renderer2D.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"
#include "UI/UIComponents.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>
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

constexpr ImGuiWindowFlags WorkspaceContainerFlags =
    ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoCollapse
    | ImGuiWindowFlags_NoSavedSettings;

constexpr ImGuiWindowFlags ToolbarFlags =
    ImGuiWindowFlags_NoTitleBar
    | ImGuiWindowFlags_NoMove
    | ImGuiWindowFlags_NoResize
    | ImGuiWindowFlags_NoCollapse
    | ImGuiWindowFlags_NoScrollbar
    | ImGuiWindowFlags_NoSavedSettings;

bool ConfigureEditorFont()
{
    ImGuiIO& io = ImGui::GetIO();
    const auto fontDirectory = Platform::GetSystemFontDirectory();
    if (fontDirectory)
    {
        // Borrow installed fonts for local display; no system font is copied into the engine
        // package.
        for (const auto* name : {"msyh.ttc", "msyh.ttf", "simsun.ttc", "segoeui.ttf"})
        {
            const auto path = fontDirectory.Value() / name;
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error) || error)
                continue;
            if (ImFont* font =
                    io.Fonts->AddFontFromFileTTF(FileSystem::PathToUtf8(path).c_str(), 17.0f))
            {
                io.FontDefault = font;
                return std::string_view(name) != "segoeui.ttf";
            }
        }
    }
    ImFontConfig fallback;
    fallback.SizePixels = 17.0f;
    io.FontDefault = io.Fonts->AddFontDefaultVector(&fallback);
    return false;
}


} // namespace

EditorApplication::EditorApplication(
    std::filesystem::path projectRoot,
    bool mcpStdio)
    : m_ProjectRoot(std::move(projectRoot)),
      m_McpStdio(mcpStdio)
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

    LoadPreferences();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    m_ImGuiContextCreated = true;
    ImGui::StyleColorsDark();
    m_ChineseFontAvailable = ConfigureEditorFont();
    if (m_Preferences.language == EditorLanguage::Chinese && !m_ChineseFontAvailable)
    {
        m_Preferences.language = EditorLanguage::English;
        m_PreferencesError = "Chinese font unavailable; using English labels.";
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.ChildRounding = 2;
    style.FrameRounding = 3;
    style.TabRounding = 3;
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;
    style.WindowPadding = ImVec2{10, 8};
    style.FramePadding = ImVec2{8, 4};
    style.ItemSpacing = ImVec2{7, 6};
    style.Colors[ImGuiCol_WindowBg] = ImVec4{.12f, .14f, .17f, 1};
    style.Colors[ImGuiCol_ChildBg] = ImVec4{.105f, .125f, .15f, 1};
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4{.105f, .12f, .14f, 1};
    style.Colors[ImGuiCol_TitleBg] = ImVec4{.145f, .17f, .20f, 1};
    style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg];
    style.Colors[ImGuiCol_Header] = ImVec4{.16f, .29f, .43f, 1};
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4{.20f, .32f, .44f, 1};
    style.Colors[ImGuiCol_HeaderActive] = ImVec4{.20f, .37f, .55f, 1};
    style.Colors[ImGuiCol_Button] = ImVec4{.15f, .18f, .215f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4{.21f, .29f, .38f, 1};
    style.Colors[ImGuiCol_ButtonActive] = ImVec4{.20f, .37f, .55f, 1};
    style.Colors[ImGuiCol_FrameBg] = ImVec4{.105f, .125f, .15f, 1};
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4{.16f, .21f, .27f, 1};
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4{.17f, .25f, .34f, 1};
    style.Colors[ImGuiCol_Tab] = ImVec4{.12f, .145f, .175f, 1};
    style.Colors[ImGuiCol_TabHovered] = ImVec4{.20f, .29f, .39f, 1};
    style.Colors[ImGuiCol_TabSelected] = ImVec4{.19f, .23f, .28f, 1};
    style.Colors[ImGuiCol_TabSelectedOverline] = ImVec4{.27f, .62f, .94f, 1};
    style.TabBarOverlineSize = 2;
    style.Colors[ImGuiCol_CheckMark] = ImVec4{.33f, .70f, 1.0f, 1};
    style.Colors[ImGuiCol_Border] = ImVec4{.25f, .29f, .34f, 1};
    style.Colors[ImGuiCol_Text] = ImVec4{.89f, .93f, .97f, 1};
    style.Colors[ImGuiCol_TextDisabled] = ImVec4{.59f, .66f, .73f, 1};
    m_UiScale = std::max(1.0f, SDL_GetWindowDisplayScale(nativeWindow)) * m_UserScale;
    style.ScaleAllSizes(m_UiScale);
    style.FontScaleDpi = m_UiScale;

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

    auto session =
        ProjectSession::Open(project, application.GetRenderer2D(), application.GetLogStore());
    if (!session)
    {
        const Error error = session.GetError();
        ShutdownImGui(application);
        return Result<void>::Failure(error);
    }

    m_ProjectSession = std::move(session).Value();
    m_ProjectSettingsPanel = std::make_unique<ProjectSettingsPanel>();

    m_EditorContext = std::make_unique<EditorContext>();
    m_EditorContext->language = m_Preferences.language;
    m_EditorContext->panelExpanded = &m_Preferences.panelExpanded;
    m_EditorContext->project = m_ProjectSession.get();
    m_EditorContext->renderer = &application.GetRenderer2D();
    m_EditorActions =
        std::make_unique<EditorActions>(*m_EditorContext);
    m_EditorConsole = std::make_unique<EditorConsole>(m_ProjectSession->GetLogStore());
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

    SetEditorLanguage(m_Preferences.language);
    const auto projectKey = ResolveEditorPreferenceProjectKey(m_ProjectSession->GetProjectRoot());
    if (projectKey)
        m_PreferencesProjectKey = projectKey.Value();
    else
        m_PreferencesError = projectKey.GetError().message;
    const auto rememberedProject = m_Preferences.RememberProject(m_PreferencesProjectKey);
    if (!rememberedProject)
        m_PreferencesError = rememberedProject.GetError().message;
    if (!m_PreferencesError.empty())
        m_EditorConsole->PushError({ErrorCode::InvalidState, m_PreferencesError});
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

    if (m_McpStdio)
    {
        m_McpPermissionPolicy =
            std::make_unique<MCP::AllowAllMcpPermissionPolicy>();

        auto host =
            McpEditorHost::Create(
                *m_ProjectSession,
                std::cin,
                std::cout,
                *m_McpPermissionPolicy);

        if (!host)
        {
            const Error error =
                host.GetError();
            OnShutdown(application);
            return Result<void>::Failure(
                error);
        }

        m_McpHost =
            std::move(host).Value();

        const auto started =
            m_McpHost->Start();

        if (!started)
        {
            const Error error =
                started.GetError();
            OnShutdown(application);
            return Result<void>::Failure(
                error);
        }
    }

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

void EditorApplication::OnEvent(const Event& event, Application&)
{
    if (std::holds_alternative<WindowFocusLostEvent>(event) ||
        std::holds_alternative<WindowResizeEvent>(event))
        m_TransformDrag.Cancel();
}

CloseDecision EditorApplication::OnCloseRequested(Application&)
{
    m_TransformDrag.Cancel();
    m_CloseRequested = true;
    return CloseDecision::Defer;
}

void EditorApplication::OnUpdate(
    TimeStep timeStep,
    Application& application)
{
    auto& window = application.GetWindow();
    auto& renderer = application.GetRenderer2D();
    const bool profileFrame = m_ProjectSession && m_ProjectSession->BeginDiagnosticsFrame();

    // Settle the final frame of Inspector input before acquiring the close guard. Do not pump
    // Agent work between the native close request and that owner-thread boundary.
    if (m_McpHost != nullptr && !m_CloseRequested)
    {
        const auto pumped =
            m_McpHost->Pump();

        if (!pumped)
        {
            const Error error =
                pumped.GetError();
            RecordError(error);
            m_McpHost->Stop();
            m_McpHost.reset();
            m_McpPermissionPolicy.reset();
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    if (m_GameInputActive)
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    else
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const float desiredScale =
        std::max(1.0f,
                 SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(window.GetNativeHandle()))) *
        m_UserScale;
    if (std::abs(desiredScale - m_UiScale) > 0.01f)
    {
        ImGui::GetStyle().ScaleAllSizes(desiredScale / m_UiScale);
        m_UiScale = desiredScale;
        ImGui::GetStyle().FontScaleDpi = desiredScale;
    }
    ImGui::NewFrame();

    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    m_WorkspaceLogicalSize = {mainViewport->WorkSize.x / m_UiScale,
                              mainViewport->WorkSize.y / m_UiScale};
    if (m_RestoreWorkspaceSize && m_WorkspaceLogicalSize.x > 0 && m_WorkspaceLogicalSize.y > 0)
    {
        const auto reference = m_Preferences.workspaceReferenceSize;
        if (reference.x > 0 && reference.y > 0)
        {
            m_WorkspacePreferences.leftWidth *= m_WorkspaceLogicalSize.x / reference.x;
            m_WorkspacePreferences.rightWidth *= m_WorkspaceLogicalSize.x / reference.x;
            m_WorkspacePreferences.utilityHeight *= m_WorkspaceLogicalSize.y / reference.y;
        }
        m_RestoreWorkspaceSize = false;
    }
    EditorWorkspaceLayout workspace =
        BuildEditorWorkspaceLayout(mainViewport->WorkSize.x / m_UiScale,
                                   mainViewport->WorkSize.y / m_UiScale, m_WorkspacePreferences);
    for (auto* rect :
         {&workspace.toolbar, &workspace.hierarchy, &workspace.viewport, &workspace.inspector,
          &workspace.utility, &workspace.assets, &workspace.diagnostics, &workspace.status})
    {
        rect->x *= m_UiScale;
        rect->y *= m_UiScale;
        rect->width *= m_UiScale;
        rect->height *= m_UiScale;
    }
    // Splitters change only Editor layout state, never Scene authoring/history.
    auto splitter = [&](const char* id, EditorPanelRect rect, bool horizontal, float& value)
    {
        ApplyWorkspaceRect(rect, *mainViewport);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{1, 1});
        ImGui::Begin(id, nullptr, WorkspaceContainerFlags | ImGuiWindowFlags_NoScrollbar);
        ImGui::InvisibleButton("##drag",
                               ImVec2{std::max(1.0f, rect.width), std::max(1.0f, rect.height)});
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeNS
                                             : ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
            value += (horizontal ? -ImGui::GetIO().MouseDelta.y : ImGui::GetIO().MouseDelta.x) /
                     m_UiScale;
        ImGui::End();
        ImGui::PopStyleVar(2);
    };
    if (workspace.hierarchy.width > 0)
        splitter("##LeftSplit",
                 {workspace.hierarchy.width, workspace.hierarchy.y, 6 * m_UiScale,
                  workspace.hierarchy.height},
                 false, m_WorkspacePreferences.leftWidth);
    float right = -m_WorkspacePreferences.rightWidth;
    if (workspace.inspector.width > 0)
        splitter("##RightSplit",
                 {workspace.inspector.x - 6 * m_UiScale, workspace.inspector.y, 6 * m_UiScale,
                  workspace.inspector.height},
                 false, right);
    m_WorkspacePreferences.rightWidth = std::clamp(-right, 250.0f, 520.0f);
    if (workspace.utility.height > 0)
        splitter("##BottomSplit",
                 {0, workspace.utility.y - 6 * m_UiScale, workspace.utility.width, 6 * m_UiScale},
                 true, m_WorkspacePreferences.utilityHeight);
    m_WorkspacePreferences.leftWidth = std::clamp(m_WorkspacePreferences.leftWidth, 170.0f, 420.0f);
    m_WorkspacePreferences.utilityHeight =
        std::clamp(m_WorkspacePreferences.utilityHeight, 120.0f, 600.0f);

    if (m_ProjectSession == nullptr
        || m_EditorCamera == nullptr
        || m_SceneRenderer == nullptr)
    {
        RecordError(
            Error{
                ErrorCode::InvalidState,
                "Editor session state is incomplete."});
    }

    // Settle an outside-click blur before toolbar or hierarchy commands observe authoring.
    if (m_ProjectSession && m_InspectorPanel && workspace.inspector.width > 0 &&
        workspace.inspector.height > 0)
    {
        ApplyWorkspaceRect(workspace.inspector, *mainViewport);
        if (const auto error = m_InspectorPanel->Draw())
            RecordError(*error);
    }

    if (m_ProjectSession != nullptr)
    {
        auto settleInspector = [&]()
        {
            const auto settled = m_InspectorPanel->CommitPendingEdit();
            if (!settled)
                RecordError(settled.GetError());
            return static_cast<bool>(settled);
        };
        auto save = [&]()
        {
            if (!settleInspector())
                return;
            const auto result = m_ProjectSession->SaveCurrentScene();
            m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human, "scene.save",
                                                              result);
            if (!result)
                RecordError(result.GetError());
            else
            {
                m_LastError.clear();
                m_EditorConsole->PushInfo("Scene saved.");
            }
        };
        auto action = [&](const Result<void>& result)
        {
            if (!result)
                RecordError(result.GetError());
            else
                m_LastError.clear();
        };
        const bool readOnly = m_ProjectSession->IsAuthoringReadOnly();
        const bool canSave = !readOnly && m_ProjectSession->IsDirty();
        ApplyWorkspaceRect(workspace.toolbar, *mainViewport);
        ImGui::Begin("##JanusToolbar", nullptr, ToolbarFlags | ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "File").c_str()))
            {
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Save Scene").c_str(),
                                    "Ctrl+S", false, canSave))
                    save();
                if (ImGui::MenuItem(
                        EditorLabel(m_Preferences.language, "Project Settings...").c_str()))
                    m_ShowProjectSettings = true;
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Exit").c_str(), "Alt+F4"))
                    m_CloseRequested = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "Edit").c_str()))
            {
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Undo").c_str(), "Ctrl+Z",
                                    false, m_EditorActions->CanUndo()))
                    action(m_EditorActions->Undo());
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Redo").c_str(), "Ctrl+Y",
                                    false, m_EditorActions->CanRedo()))
                    action(m_EditorActions->Redo());
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "Entity").c_str()))
            {
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Create Entity").c_str(),
                                    nullptr, false, !readOnly))
                {
                    const auto created = m_EditorActions->CreateEntity("Entity");
                    if (!created)
                        RecordError(created.GetError());
                }
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Duplicate").c_str(),
                                    "Ctrl+D", false,
                                    !readOnly && m_EditorContext->selection.HasSelection()))
                {
                    auto copied = m_EditorActions->DuplicateEntity(
                        *m_EditorContext->selection.GetSelectedUUID());
                    if (!copied)
                        RecordError(copied.GetError());
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "View").c_str()))
            {
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Frame Camera").c_str()))
                    FrameScene(false);
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Focus Selected").c_str(),
                                    "F"))
                    FrameScene(true);
                ImGui::MenuItem(EditorLabel(m_Preferences.language, "Grid").c_str(), nullptr,
                                &m_ShowGrid);
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Reset Layout").c_str()))
                    m_WorkspacePreferences =
                        GetDefaultWorkspacePreferences(m_WorkspacePreferences.mode);
                if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "Layout").c_str()))
                {
                    const std::pair<const char*, EditorLayoutMode> modes[] = {
                        {"Standard", EditorLayoutMode::Standard},
                        {"Focus", EditorLayoutMode::Focus},
                        {"Debug", EditorLayoutMode::Debug}};
                    for (const auto& [name, mode] : modes)
                    {
                        if (ImGui::MenuItem(EditorLabel(m_Preferences.language, name).c_str(),
                                            nullptr, m_WorkspacePreferences.mode == mode))
                        {
                            const auto committed = m_InspectorPanel->CommitPendingEdit();
                            if (committed)
                            {
                                m_TransformDrag.Cancel();
                                m_WorkspacePreferences = GetDefaultWorkspacePreferences(mode);
                                m_InspectorPanel->ReleaseKeyboardOwnership();
                            }
                            else
                                RecordError(committed.GetError());
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "Language").c_str()))
                {
                    if (ImGui::MenuItem("English###English", nullptr,
                                        m_Preferences.language == EditorLanguage::English))
                        SetEditorLanguage(EditorLanguage::English);
                    if (ImGui::MenuItem(m_ChineseFontAvailable
                                            ? "简体中文###Chinese"
                                            : "Chinese (font unavailable)###Chinese",
                                        nullptr, m_Preferences.language == EditorLanguage::Chinese,
                                        m_ChineseFontAvailable))
                        SetEditorLanguage(EditorLanguage::Chinese);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "UI Scale").c_str()))
                {
                    for (const float scale : {0.75f, 1.0f, 1.25f, 1.5f, 2.0f})
                    {
                        const auto label = std::to_string(static_cast<int>(scale * 100)) + "%";
                        if (ImGui::MenuItem(label.c_str(), nullptr, m_UserScale == scale))
                            m_UserScale = scale;
                    }
                    ImGui::EndMenu();
                }
                if (!m_PreferencesError.empty())
                {
                    ImGui::Separator();
                    ImGui::TextWrapped("%s", m_PreferencesError.c_str());
                    if (ImGui::MenuItem(
                            EditorLabel(m_Preferences.language, "Retry saving preferences")
                                .c_str()))
                    {
                        m_PreferencesPending = true;
                        UpdatePreferences(true);
                        if (!m_PreferencesSaveFailed)
                            m_PreferencesError.clear();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(EditorLabel(m_Preferences.language, "Help").c_str()))
            {
                if (ImGui::MenuItem(EditorLabel(m_Preferences.language, "Editor Controls").c_str()))
                    m_ShowAbout = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        const float buttonWidth = 80 * m_UiScale;
        auto toolSeparator = [&]()
        {
            ImGui::SameLine(0, 14 * m_UiScale);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine({pos.x, pos.y + 3 * m_UiScale},
                                                {pos.x, pos.y + 29 * m_UiScale},
                                                ImGui::GetColorU32(ImGuiCol_Border));
            ImGui::Dummy({1, 32 * m_UiScale});
            ImGui::SameLine(0, 14 * m_UiScale);
        };
        auto tool = [&](Icon icon, const char* label, bool enabled)
        {
            ImGui::BeginDisabled(!enabled);
            const bool clicked =
                IconButton(icon, EditorLabel(m_Preferences.language, label).c_str(),
                           ImVec2{buttonWidth, 32 * m_UiScale});
            ImGui::EndDisabled();
            return clicked;
        };
        if (tool(Icon::Save, "Save", canSave))
            save();
        ImGui::SameLine();
        if (tool(Icon::Undo, "Undo", m_EditorActions->CanUndo()))
            action(m_EditorActions->Undo());
        ImGui::SameLine();
        if (tool(Icon::Redo, "Redo", m_EditorActions->CanRedo()))
            action(m_EditorActions->Redo());
        toolSeparator();
        const auto runtime = m_ProjectSession->GetRuntimeState();
        if (tool(Icon::Play, "Play", !readOnly) && settleInspector())
        {
            const auto result = m_ProjectSession->StartRuntime(m_GameInput);
            m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human, "runtime.play",
                                                              result);
            action(result);
            if (result)
            {
                m_ReturnToGameView = m_WasGameView;
                m_SelectGameViewTab = true;
                m_SelectSceneViewTab = false;
            }
        }
        ImGui::SameLine();
        if (tool(runtime == RuntimeState::Paused ? Icon::Play : Icon::Pause,
                 runtime == RuntimeState::Paused ? "Resume" : "Pause",
                 runtime == RuntimeState::Playing || runtime == RuntimeState::Paused))
        {
            const auto result = runtime == RuntimeState::Paused ? m_ProjectSession->ResumeRuntime()
                                                                : m_ProjectSession->PauseRuntime();
            m_ProjectSession->GetCommandBus().RecordOperation(
                CommandActor::Human,
                runtime == RuntimeState::Paused ? "runtime.play" : "runtime.pause", result);
            action(result);
        }
        ImGui::SameLine();
        if (tool(Icon::Step, "Step", runtime == RuntimeState::Paused))
        {
            const auto result = m_ProjectSession->StepRuntime();
            m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human, "runtime.step",
                                                              result);
            action(result);
        }
        ImGui::SameLine();
        if (tool(Icon::Stop, "Stop", m_ProjectSession->IsPlaying()))
        {
            const auto result = m_ProjectSession->StopRuntime();
            m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human, "runtime.stop",
                                                              result);
            action(result);
            if (result)
            {
                m_SelectSceneViewTab = !m_ReturnToGameView;
                m_SelectGameViewTab = m_ReturnToGameView;
            }
        }
        toolSeparator();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(EditorText(
            m_Preferences.language, RuntimeStateName(m_ProjectSession->GetRuntimeState()).data()));
        ImGui::End();

        // Text fields own editing shortcuts; Game focus keeps gameplay keys isolated.
        if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
            !(m_InspectorPanel && m_InspectorPanel->OwnsKeyboardInput()) && !m_GameInputActive)
        {
            if (!readOnly && m_EditorContext->selection.HasSelection() &&
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal))
            {
                auto copied =
                    m_EditorActions->DuplicateEntity(*m_EditorContext->selection.GetSelectedUUID());
                if (!copied)
                    RecordError(copied.GetError());
            }
            if (canSave && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
                save();
            if (m_EditorActions->CanUndo() &&
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
                action(m_EditorActions->Undo());
            if (m_EditorActions->CanRedo() &&
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal))
                action(m_EditorActions->Redo());
        }
        ApplyWorkspaceRect(workspace.status, *mainViewport);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{12 * m_UiScale, 3 * m_UiScale});
        ImGui::Begin("##Status", nullptr, ToolbarFlags);
        ImGui::Text(EditorText(m_Preferences.language, "%s%s"),
                    m_ProjectSession->GetEditorScene().GetMetadata().name.c_str(),
                    EditorText(m_Preferences.language, m_ProjectSession->IsDirty()
                                                           ? " *  |  Unsaved changes"
                                                           : "  |  Saved"));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", m_ProjectSession->GetCurrentScenePath().string().c_str());
        if (!m_PreferencesError.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored({1, .75f, .3f, 1}, "%s",
                               EditorText(m_Preferences.language, "Preferences warning"));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", m_PreferencesError.c_str());
        }
        if (workspace.status.width > 950 * m_UiScale)
        {
            ImGui::SameLine(workspace.status.width - 390 * m_UiScale);
            ImGui::Text(
                EditorText(m_Preferences.language, "Entities: %zu  |  Assets: %zu  |  MCP: %s"),
                m_ProjectSession->GetEditorScene().GetEntities().size(),
                m_ProjectSession->GetAssetRegistry().Size(), m_McpHost ? "stdio enabled" : "off");
        }
        ImGui::End();
        ImGui::PopStyleVar();
        if (m_ShowProjectSettings)
        {
            ImGui::SetNextWindowSize(ImVec2{620 * m_UiScale, 570 * m_UiScale},
                                     ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Project Settings", &m_ShowProjectSettings))
                m_ProjectSettingsPanel->Draw(*m_ProjectSession);
            ImGui::End();
        }
        if (m_ShowAbout)
        {
            ImGui::SetNextWindowSize(ImVec2{440 * m_UiScale, 250 * m_UiScale},
                                     ImGuiCond_FirstUseEver);
            if (ImGui::Begin(EditorLabel(m_Preferences.language, "Editor Controls").c_str(),
                             &m_ShowAbout))
            {
                ImGui::TextWrapped(EditorText(
                    m_Preferences.language,
                    "Scene: click to select, middle-drag to pan, wheel to zoom. F focuses the "
                    "selected entity. Frame Camera restores the game camera region."));
                ImGui::TextWrapped(
                    EditorText(m_Preferences.language,
                               "Q selects, W moves. Drag X/Y arrows or the square plane handle. "
                               "Hold Ctrl to snap movement to the displayed major grid spacing. "
                               "Esc cancels; release commits one Undo step. UI uses Inspector "
                               "layout fields."));
                ImGui::Separator();
                ImGui::TextWrapped(EditorText(
                    m_Preferences.language,
                    "Drag panel separators to resize. View > Reset Layout restores defaults. "
                    "Resource fields accept registered assets of the matching type."));
            }
            ImGui::End();
        }
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

        if (workspace.hierarchy.width > 0 && workspace.hierarchy.height > 0)
        {
            ApplyWorkspaceRect(workspace.hierarchy, *mainViewport);
            const auto hierarchyError = m_HierarchyPanel->Draw();
            if (hierarchyError.has_value())
            {
                RecordError(*hierarchyError);
            }
        }
    }

    if (m_AssetBrowserPanel != nullptr && m_ConsolePanel != nullptr && workspace.utility.height > 0)
    {
        const bool split = workspace.diagnostics.width > 0;
        ApplyWorkspaceRect(workspace.assets, *mainViewport);
        ImGui::Begin("##ProjectWorkspace", nullptr, WorkspaceContainerFlags);
        if (split)
        {
            IconText(Icon::Folder, EditorText(m_Preferences.language, "Project"));
            ImGui::Separator();
            const auto error = m_AssetBrowserPanel->DrawContents();
            if (error)
                RecordError(*error);
            ImGui::End();
            ApplyWorkspaceRect(workspace.diagnostics, *mainViewport);
            ImGui::Begin("##DiagnosticsWorkspace", nullptr, WorkspaceContainerFlags);
        }
        if (ImGui::BeginTabBar("UtilityTabs"))
        {
            if (!split &&
                IconTab(Icon::Folder, EditorLabel(m_Preferences.language, "Project").c_str(),
                        m_EditorContext->locateAsset ? ImGuiTabItemFlags_SetSelected : 0))
            {
                const auto error = m_AssetBrowserPanel->DrawContents();
                if (error)
                    RecordError(*error);
                ImGui::EndTabItem();
            }
            if (IconTab(Icon::Console, EditorLabel(m_Preferences.language, "Console").c_str(),
                        m_SelectConsoleTab ? ImGuiTabItemFlags_SetSelected : 0))
            {
                m_SelectConsoleTab = false;
                m_ConsolePanel->DrawContents();
                ImGui::EndTabItem();
            }

            if (IconTab(Icon::Agent, EditorLabel(m_Preferences.language, "Agent Activity").c_str()))
            {
                auto& commands = m_ProjectSession->GetCommandBus();
                if (commands.HasTransaction())
                {
                    ImGui::Text(
                        EditorText(m_Preferences.language,
                                   "Provisional transaction: %zu commands, %zu bytes reserved"),
                        commands.GetPendingCount(), commands.GetPendingBytes());
                    if (!commands.RecoveryRequired() &&
                        ImGui::Button(
                            EditorLabel(m_Preferences.language, "Cancel Agent transaction")
                                .c_str()))
                        m_ProjectSession->CancelAuthoringTransaction(
                            m_ProjectSession->GetTransactionOwner(), CommandActor::Human);
                }
                if (commands.RecoveryRequired())
                {
                    ImGui::TextWrapped(EditorText(
                        m_Preferences.language,
                        "Authoring recovery required. Writes, Save and Play are frozen."));
                    if (ImGui::Button(EditorLabel(m_Preferences.language,
                                                  "Discard unsaved changes and reload...")
                                          .c_str()))
                        ImGui::OpenPopup(
                            EditorLabel(m_Preferences.language, "Confirm authoring recovery")
                                .c_str());
                }
                if (ImGui::BeginPopupModal(
                        EditorLabel(m_Preferences.language, "Confirm authoring recovery").c_str(),
                        nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::TextUnformatted(EditorText(
                        m_Preferences.language,
                        "Discard all unsaved authoring changes and reload the scene from disk?"));
                    if (ImGui::Button(
                            EditorLabel(m_Preferences.language, "Discard and reload").c_str()))
                    {
                        const auto recovered = m_ProjectSession->DiscardUnsavedAndReload();
                        if (!recovered)
                            RecordError(recovered.GetError());
                        else if (m_EditorContext)
                            m_EditorContext->selection.Clear();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(
                            EditorLabel(m_Preferences.language, "Keep recovery state").c_str()))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                ImGui::Separator();
                ImGui::BeginChild("ActivityEntries");
                const auto& activity = commands.GetActivity();
                const usize start = activity.size() > 200 ? activity.size() - 200 : 0;
                for (usize i = start; i < activity.size(); ++i)
                {
                    const auto& row = activity[i];
                    ImGui::TextWrapped(
                        EditorText(m_Preferences.language, "#%llu %s %s | command %llu | %s"),
                        row.sequence, CommandActorName(row.actor).data(),
                        CommandOutcomeName(row.outcome).data(), row.commandId,
                        row.description.c_str());
                    ImGui::PushID(std::to_string(row.sequence).c_str());
                    int effectIndex = 0;
                    for (const auto& effect : row.effects)
                    {
                        ImGui::PushID(effectIndex++);
                        const auto entity =
                            m_ProjectSession->GetEditorScene().FindEntity(effect.entity);
                        ImGui::BeginDisabled(!entity.IsValid());
                        const auto target = effect.operation + " " + effect.entity.ToString();
                        if (ImGui::Selectable(target.c_str(), false))
                        {
                            const auto settled = m_InspectorPanel->CommitPendingEdit();
                            if (!settled)
                                RecordError(settled.GetError());
                            else
                            {
                                m_TransformDrag.Cancel();
                                m_EditorContext->selection.Select(effect.entity);
                                m_SelectSceneViewTab = true;
                                FrameScene(true);
                            }
                        }
                        ImGui::EndDisabled();
                        if (!entity.IsValid() &&
                            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip(
                                "%s",
                                EditorText(m_Preferences.language,
                                           "Entity no longer exists in the authoring scene."));
                        ImGui::PopID();
                    }
                    ImGui::PopID();
                    if (row.truncated)
                        ImGui::TextDisabled("  Details truncated (%zu effects total)",
                                            row.effectCount);
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (IconTab(Icon::Profiler, EditorLabel(m_Preferences.language, "Profiler").c_str()))
            {
                bool enabled = m_ProjectSession->GetProfiler().IsEnabled();
                if (ImGui::Checkbox(
                        EditorLabel(m_Preferences.language, "Record CPU frames").c_str(), &enabled))
                    m_ProjectSession->GetProfiler().SetEnabled(enabled);
                const auto& frame = m_ProjectSession->GetDiagnosticsFrame();
                if (frame)
                {
                    ImGui::Text(EditorText(m_Preferences.language, "CPU frame %llu: %.3f ms"),
                                frame->cpu.frameId, frame->cpu.durationMilliseconds);
                    ImGui::TextUnformatted(EditorText(
                        m_Preferences.language,
                        "Inclusive scopes overlap. CPU timings do not measure GPU time."));
                    for (const auto& scope : frame->cpu.scopes)
                        ImGui::Text(EditorText(m_Preferences.language, "%s: %.3f ms"),
                                    scope.name.c_str(), scope.durationMilliseconds);
                    const auto showPass =
                        [this](const char* name, const std::optional<RenderPassSnapshot>& pass)
                    {
                        if (pass)
                            ImGui::Text(EditorText(m_Preferences.language,
                                                   "%s: %zu entities, %u sprites, %u draws"),
                                        name, pass->entityCount, pass->statistics.spriteCount,
                                        pass->statistics.drawCallCount);
                        else
                            ImGui::Text(EditorText(m_Preferences.language, "%s: unavailable"),
                                        name);
                    };
                    showPass("Scene", frame->sceneView);
                    showPass("Game", frame->gameView);
                }
                else
                    ImGui::TextUnformatted(
                        EditorText(m_Preferences.language, "No completed frame."));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    bool renderSceneView = false;
    bool renderGameView = false;
    bool acceptGameInput = false;
    InputState gameInput;
    std::optional<Vector2> pendingScenePick;

    if (m_ProjectSession != nullptr
        && m_EditorCamera != nullptr
        && m_SceneRenderer != nullptr)
    {
        ApplyWorkspaceRect(
            workspace.viewport,
            *mainViewport);

        ImGui::Begin(
            "##ViewportWorkspace",
            nullptr,
            WorkspaceContainerFlags);

        const auto fault = m_ProjectSession->GetRuntimeStatus();
        if (fault.state == RuntimeState::Faulted)
        {
            ImGui::TextColored(
                {1, .4f, .4f, 1}, "%s",
                EditorText(m_Preferences.language, "Runtime faulted. Stop to return to editing."));
            if (fault.lastError)
            {
                ImGui::BeginChild("FaultSummary", {0, ImGui::GetTextLineHeightWithSpacing() * 2});
                ImGui::TextWrapped("%s", fault.lastError->message.c_str());
                ImGui::EndChild();
            }
            if (ImGui::Button(EditorLabel(m_Preferences.language, "Locate error").c_str()))
            {
                m_ConsolePanel->FocusRuntimeError(fault.runtimeId, fault.lastError
                                                                       ? fault.lastError->message
                                                                       : std::string_view{});
                m_SelectConsoleTab = true;
                if (m_WorkspacePreferences.mode == EditorLayoutMode::Focus)
                    m_WorkspacePreferences =
                        GetDefaultWorkspacePreferences(EditorLayoutMode::Debug);
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLabel(m_Preferences.language, "Stop runtime").c_str()))
            {
                const auto stopped = m_ProjectSession->StopRuntime();
                m_ProjectSession->GetCommandBus().RecordOperation(CommandActor::Human,
                                                                  "runtime.stop", stopped);
                if (!stopped)
                    RecordError(stopped.GetError());
                else
                {
                    m_SelectSceneViewTab = !m_ReturnToGameView;
                    m_SelectGameViewTab = m_ReturnToGameView;
                }
            }
            ImGui::Separator();
        }

        if (ImGui::BeginTabBar("ViewportTabs"))
        {
            const ImGuiTabItemFlags sceneTabFlags =
                m_SelectSceneViewTab
                    ? ImGuiTabItemFlags_SetSelected
                    : ImGuiTabItemFlags_None;

            const bool sceneTabVisible = IconTab(
                Icon::Entity, EditorLabel(m_Preferences.language, "Scene").c_str(), sceneTabFlags);

            m_SelectSceneViewTab = false;

            if (sceneTabVisible)
            {
                m_WasGameView = false;
                if (IconOnlyButton(Icon::Frame,
                                   EditorLabel(m_Preferences.language, "Frame Camera").c_str()))
                    FrameScene(false);
                ImGui::SameLine();
                if (IconOnlyButton(
                        Icon::Focus,
                        EditorLabel(m_Preferences.language, "Focus Selected (F)").c_str()))
                    FrameScene(true);
                ImGui::SameLine();
                ImGui::Checkbox(EditorLabel(m_Preferences.language, "Grid").c_str(), &m_ShowGrid);
                ImGui::SameLine();
                if (IconOnlyButton(Icon::Select,
                                   EditorLabel(m_Preferences.language,
                                               m_MoveTool ? "Select (Q)" : "Select (Q) *")
                                       .c_str()))
                {
                    m_MoveTool = false;
                    m_TransformDrag.Cancel();
                }
                ImGui::SameLine();
                if (IconOnlyButton(Icon::Move, EditorLabel(m_Preferences.language,
                                                           m_MoveTool ? "Move (W) *" : "Move (W)")
                                                   .c_str()))
                    m_MoveTool = true;

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

                    if (m_InitialFrame)
                    {
                        const auto* savedCamera = m_Preferences.FindCamera(
                            m_PreferencesProjectKey, m_ProjectSession->GetCurrentScenePath());
                        if (!savedCamera ||
                            !m_EditorCamera->RestoreView(savedCamera->position, savedCamera->zoom))
                            FrameScene(false);
                        m_InitialFrame = false;
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

                        const auto viewMin = ImGui::GetItemRectMin();
                        const auto viewMax = ImGui::GetItemRectMax();
                        const bool hovered = ImGui::IsItemHovered();
                        const bool consumed = DrawSceneInteraction(
                            {viewMin.x, viewMin.y}, {available.x, available.y}, hovered);
                        const float pad = ImGui::GetStyle().WindowPadding.x;
                        char viewLabel[160];
                        std::snprintf(
                            viewLabel, sizeof(viewLabel),
                            EditorText(m_Preferences.language,
                                       "2D  |  %.3f units/px  |  %s  |  Ctrl snap: %.3g"),
                            m_EditorCamera->GetZoom(),
                            EditorText(m_Preferences.language, m_MoveTool ? "Move" : "Select"),
                            m_EditorCamera->GetGridSpacing());
                        const float labelWidth =
                            std::min(ImGui::CalcTextSize(viewLabel).x, available.x - pad * 4);
                        if (labelWidth > 0 && available.y > ImGui::GetFrameHeight() * 2)
                        {
                            const ImVec2 pos{viewMin.x + pad,
                                             viewMax.y - ImGui::GetFontSize() - pad * 2};
                            auto* draw = ImGui::GetWindowDrawList();
                            draw->AddRectFilled(pos,
                                                {pos.x + labelWidth + pad * 2, viewMax.y - pad},
                                                IM_COL32(20, 25, 32, 220), 3);
                            DrawEllipsizedText(draw, {pos.x + pad, pos.y + pad * .5f}, labelWidth,
                                               ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                               viewLabel);
                        }
                        if (hovered && !consumed && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            const auto mouse = ImGui::GetMousePos();
                            pendingScenePick = Vector2{
                                (mouse.x - viewMin.x) * m_SceneViewViewport.width / available.x,
                                (mouse.y - viewMin.y) * m_SceneViewViewport.height / available.y};
                        }

                        renderSceneView = true;
                    }
                    else
                    {
                        RecordError(presentation.GetError());
                    }
                }

                ImGui::EndTabItem();
            }

            const ImGuiTabItemFlags gameTabFlags =
                m_SelectGameViewTab
                    ? ImGuiTabItemFlags_SetSelected
                    : ImGuiTabItemFlags_None;

            const bool gameTabVisible = IconTab(
                Icon::Play, EditorLabel(m_Preferences.language, "Game").c_str(), gameTabFlags);

            m_SelectGameViewTab = false;

            if (gameTabVisible)
            {
                m_WasGameView = true;
                const ImVec2 available =
                    ImGui::GetContentRegionAvail();
                const EditorPanelRect fitted = FitAspectRatio(
                    available.x, available.y,
                    static_cast<f32>(m_ProjectSession->GetProjectSettings().width) /
                        static_cast<f32>(m_ProjectSession->GetProjectSettings().height));

                if (HasUsableContentSize(
                        ImVec2{fitted.width, fitted.height}))
                {
                    const ImVec2 origin =
                        ImGui::GetCursorPos();

                    ImGui::SetCursorPos(
                        ImVec2{
                            origin.x + fitted.x,
                            origin.y + fitted.y});

                    const Viewport requested =
                        ToViewport(
                            ImVec2{
                                fitted.width,
                                fitted.height});

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
                            ImVec2{
                                fitted.width,
                                fitted.height},
                            ImVec2{0.0f, 1.0f},
                            ImVec2{1.0f, 0.0f});
                        const auto item = ImGui::GetItemRectMin();
                        const auto windowOrigin = ImGui::GetMainViewport()->Pos;
                        const auto& settings = m_ProjectSession->GetProjectSettings();
                        acceptGameInput = AcceptGameViewInput(
                            m_InspectorPanel && m_InspectorPanel->OwnsKeyboardInput());
                        gameInput = application.GetInput().ForViewport(
                            {item.x - windowOrigin.x, item.y - windowOrigin.y},
                            {fitted.width, fitted.height},
                            {static_cast<f32>(settings.width), static_cast<f32>(settings.height)},
                            acceptGameInput);
                        renderGameView = true;
                    }
                    else
                    {
                        RecordError(presentation.GetError());
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    if (!renderSceneView || m_CloseRequested)
        m_TransformDrag.Cancel();
    DrawCloseConfirmation(application);
    if (m_CloseRequested || (m_CloseController && m_CloseController->IsPending()))
        m_SuppressGameUntilReleased = true;
    if (m_SuppressGameUntilReleased)
    {
        acceptGameInput = false;
        bool held = false;
        for (usize key = 0; key < static_cast<usize>(KeyCode::Count); ++key)
            held = held || application.GetInput().IsKeyDown(static_cast<KeyCode>(key));
        for (usize button = 0; button < static_cast<usize>(PointerButton::Count); ++button)
            held = held ||
                   application.GetInput().IsPointerButtonDown(static_cast<PointerButton>(button));
        if (!held && !m_CloseRequested && !(m_CloseController && m_CloseController->IsPending()))
            m_SuppressGameUntilReleased = false;
    }

    if (acceptGameInput)
        m_GameInput = gameInput;
    else if (m_GameInputActive)
    {
        // Losing the viewport releases held controls once; tool panels cannot feed gameplay.
        m_GameInput.BeginFrame();
        m_GameInput.Apply(WindowFocusLostEvent{});
    }
    else
        m_GameInput = {};
    m_GameInputActive = acceptGameInput;

    if (m_ProjectSession != nullptr &&
        m_ProjectSession->GetRuntimeState() == RuntimeState::Playing &&
        !(m_CloseController && m_CloseController->IsAccepted()))
    {
        const auto updated =
            m_ProjectSession->UpdateRuntime(timeStep);
        if (!updated)
        {
            m_LastError = updated.GetError().message;

            // Keep the faulted world available for diagnostics until explicit Stop.
        }
    }

    if (renderSceneView)
    {
        CpuScope profile(m_ProjectSession->GetProfiler(), "SceneView.Render");
        Scene& scene =
            ResolveSceneViewScene(*m_ProjectSession);

        const auto rendered =
            m_SceneRenderer->Render(SceneRenderRequest{scene,
                                                       m_ProjectSession->GetAssetService(),
                                                       renderer,
                                                       m_EditorCamera->ToRenderCamera(),
                                                       m_SceneViewViewport,
                                                       m_SceneViewTarget,
                                                       {},
                                                       false,
                                                       nullptr,
                                                       nullptr,
                                                       Color{0.14f, 0.18f, 0.23f, 1.0f},
                                                       m_TransformDrag.GetPreview()});

        if (rendered)
            m_ProjectSession->CaptureRenderPass(false, renderer.GetStatistics(),
                                                scene.GetEntities().size());
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
        CpuScope profile(m_ProjectSession->GetProfiler(), "GameView.Render");
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
            const auto rendered = m_SceneRenderer->Render(
                SceneRenderRequest{scene,
                                   m_ProjectSession->GetAssetService(),
                                   renderer,
                                   camera.Value(),
                                   m_GameViewViewport,
                                   m_GameViewTarget,
                                   {m_ProjectSession->GetProjectSettings().width,
                                    m_ProjectSession->GetProjectSettings().height},
                                   true,
                                   m_ProjectSession->GetRuntimeSession()
                                       ? &m_ProjectSession->GetRuntimeSession()->GetUIState()
                                       : nullptr,
                                   m_ProjectSession->GetRuntimeSession()
                                       ? &m_ProjectSession->GetRuntimeSession()->GetAnimations()
                                       : nullptr});

            if (rendered)
                m_ProjectSession->CaptureRenderPass(true, renderer.GetStatistics(),
                                                    scene.GetEntities().size());
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

    UpdatePreferences();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (profileFrame)
        m_ProjectSession->EndDiagnosticsFrame();

    // Escape belongs to the active ImGui control or game input, never application exit.
}

void EditorApplication::FrameScene(bool selectedOnly)
{
    m_TransformDrag.Cancel();
    if (!m_ProjectSession || !m_EditorCamera || !m_SceneRenderer)
        return;
    auto& scene = m_ProjectSession->GetEditorScene();
    const auto camera = m_SceneRenderer->ResolvePrimaryCamera(scene);
    if (selectedOnly && m_EditorContext)
    {
        const auto entity = m_EditorContext->selection.Resolve(scene);
        const auto* transform = scene.GetComponent<TransformComponent>(entity);
        const auto* sprite = scene.GetComponent<SpriteRendererComponent>(entity);
        if (transform && !scene.HasComponent<UIRectComponent>(entity))
        {
            Vector2 size{4, 4};
            if (sprite)
                size = {std::max(2.0f, std::abs(sprite->size.x * transform->worldScale.x) * 3),
                        std::max(2.0f, std::abs(sprite->size.y * transform->worldScale.y) * 3)};
            m_EditorCamera->Frame(transform->worldPosition, size, m_SceneViewViewport);
        }
    }
    else if (camera)
    {
        const auto& settings = m_ProjectSession->GetProjectSettings();
        m_EditorCamera->Frame(
            camera.Value().position,
            {settings.width * camera.Value().zoom, settings.height * camera.Value().zoom},
            m_SceneViewViewport);
    }
}

void EditorApplication::DrawCloseConfirmation(Application& application)
{
    if (!m_ProjectSession)
    {
        if (m_CloseRequested)
            application.RequestExit();
        return;
    }
    if (m_CloseRequested && m_CloseController && m_CloseController->IsPending())
        m_CloseRequested = false; // Repeated native requests must not reset the user's choices.
    if (m_CloseRequested && !m_CloseNeedsDraftDecision)
    {
        const auto settled = m_InspectorPanel->CommitPendingEdit();
        if (!settled)
        {
            RecordError(settled.GetError());
            m_CloseNeedsDraftDecision = true;
            ImGui::OpenPopup(
                EditorLabel(m_Preferences.language, "Uncommitted Inspector edit").c_str());
        }
        else
        {
            if (!m_CloseController)
                m_CloseController = std::make_unique<EditorCloseController>(*m_ProjectSession);
            m_CloseController->Request();
            m_CloseRequested = false;
            m_CloseStopRuntime = false;
            m_CloseSaveSettings = true;
            m_CloseDiscardSettings = false;
            const auto& commands = m_ProjectSession->GetCommandBus();
            if (!m_ProjectSession->IsDirty() && !m_ProjectSession->HasRuntime() &&
                !commands.HasTransaction() && !commands.RecoveryRequired() &&
                !m_ProjectSettingsPanel->HasUnsavedChanges(*m_ProjectSession))
            {
                if (m_CloseController->Confirm(false, false, nullptr))
                    application.RequestExit();
            }
            else
                ImGui::OpenPopup(EditorLabel(m_Preferences.language, "Close Janus Editor").c_str());
        }
    }
    auto fitModal = []
    {
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
        ImGui::SetNextWindowSizeConstraints({std::min(420.0f, viewport->WorkSize.x - 24), 0},
                                            {std::max(200.0f, viewport->WorkSize.x - 24),
                                             std::max(160.0f, viewport->WorkSize.y - 24)});
    };
    fitModal();
    if (ImGui::BeginPopupModal(
            EditorLabel(m_Preferences.language, "Uncommitted Inspector edit").c_str(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            EditorText(m_Preferences.language,
                       "The current field could not be committed. Cancel to correct it, or "
                       "explicitly discard this field draft."));
        ImGui::TextWrapped("%s", m_LastError.c_str());
        if (ImGui::Button(EditorLabel(m_Preferences.language, "Cancel close").c_str(), {-1, 0}) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_CloseRequested = false;
            m_CloseNeedsDraftDecision = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button(
                EditorLabel(m_Preferences.language, "Discard field draft and review close").c_str(),
                {-1, 0}))
        {
            m_InspectorPanel->DiscardPendingEdit();
            m_CloseNeedsDraftDecision = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    fitModal();
    if (ImGui::BeginPopupModal(EditorLabel(m_Preferences.language, "Close Janus Editor").c_str(),
                               nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto& commands = m_ProjectSession->GetCommandBus();
        const bool recovery = commands.RecoveryRequired();
        const bool transaction = commands.HasTransaction() && !recovery;
        const bool settingsDirty = m_ProjectSettingsPanel->HasUnsavedChanges(*m_ProjectSession);
        ImGui::TextWrapped(EditorText(m_Preferences.language, "Scene: %s"),
                           m_ProjectSession->GetCurrentScenePath().generic_string().c_str());
        ImGui::TextUnformatted(EditorText(m_Preferences.language, m_ProjectSession->IsDirty()
                                                                      ? "Scene has unsaved changes."
                                                                      : "Scene is saved."));
        if (m_ProjectSession->HasRuntime())
        {
            ImGui::TextWrapped(
                EditorText(m_Preferences.language,
                           "Runtime is %s. Simulation continues with gameplay input blocked "
                           "until you confirm Stop."),
                EditorText(m_Preferences.language,
                           RuntimeStateName(m_ProjectSession->GetRuntimeState()).data()));
            ImGui::Checkbox(EditorLabel(m_Preferences.language, "Stop runtime before exit").c_str(),
                            &m_CloseStopRuntime);
        }
        if (recovery)
        {
            ImGui::TextWrapped(
                EditorText(m_Preferences.language,
                           "Authoring recovery required. Saving is blocked. Discard exits "
                           "without writing this authoring state."));
            m_CloseSaveSettings = false;
        }
        if (transaction)
        {
            ImGui::TextWrapped(
                EditorText(m_Preferences.language,
                           "An Agent transaction is active. Wait for its owner, cancel close, "
                           "or explicitly roll it back."));
            if (ImGui::Button(
                    EditorLabel(m_Preferences.language, "Roll back Agent transaction").c_str(),
                    {-1, 0}))
            {
                const auto rolled = m_CloseController->RollbackTransaction();
                if (!rolled)
                    RecordError(rolled.GetError());
            }
        }
        if (settingsDirty)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(
                EditorText(m_Preferences.language, "Project settings also have an unsaved draft."));
            ImGui::BeginDisabled(recovery);
            if (ImGui::Checkbox(
                    EditorLabel(m_Preferences.language, "Save project settings before exit")
                        .c_str(),
                    &m_CloseSaveSettings) &&
                m_CloseSaveSettings)
                m_CloseDiscardSettings = false;
            ImGui::EndDisabled();
            if (ImGui::Checkbox(
                    EditorLabel(m_Preferences.language, "Discard project settings draft").c_str(),
                    &m_CloseDiscardSettings) &&
                m_CloseDiscardSettings)
                m_CloseSaveSettings = false;
        }
        if (m_CloseController->GetError())
        {
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_CloseController->GetError()->message.c_str());
            ImGui::TextWrapped(
                EditorText(m_Preferences.language,
                           "You can retry or cancel. A completed Stop or successful settings "
                           "save is retained."));
        }
        ImGui::Separator();
        if (ImGui::Button(EditorLabel(m_Preferences.language, "Cancel").c_str(), {-1, 0}) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_CloseController->Cancel();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        const bool ready = !transaction &&
                           (!m_ProjectSession->HasRuntime() || m_CloseStopRuntime) &&
                           (!settingsDirty || m_CloseSaveSettings || m_CloseDiscardSettings);
        auto confirm = [&](bool saveScene)
        {
            const auto draft = m_ProjectSettingsPanel->GetDraft();
            const auto closed =
                m_CloseController->Confirm(saveScene, m_CloseStopRuntime,
                                           settingsDirty && m_CloseSaveSettings ? &draft : nullptr);
            // A settings write may have succeeded even when a later Scene save failed.
            if (settingsDirty && m_CloseSaveSettings &&
                draft == m_ProjectSession->GetProjectSettings())
                m_ProjectSettingsPanel->AcceptSaved(draft);
            if (closed)
            {
                application.RequestExit();
                ImGui::CloseCurrentPopup();
            }
            else
                RecordError(closed.GetError());
        };
        ImGui::BeginDisabled(!ready || recovery);
        if (ImGui::Button(EditorLabel(m_Preferences.language, "Save Scene and Exit").c_str(),
                          {-1, 0}))
            confirm(true);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button(
                EditorLabel(m_Preferences.language, "Discard Scene changes and Exit").c_str(),
                {-1, 0}))
            confirm(false);
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
}

void EditorApplication::OnShutdown(Application& application) noexcept
{
    UpdatePreferences(true);
    auto& renderer = application.GetRenderer2D();

    if (m_McpHost != nullptr)
    {
        m_McpHost->Stop();
        m_McpHost.reset();
    }
    m_McpPermissionPolicy.reset();
    // Release the guard only after the protocol worker has stopped, before destroying its session.
    m_CloseController.reset();

    if (m_ProjectSession != nullptr && m_ProjectSession->HasRuntime())
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
    m_ProjectSettingsPanel.reset();

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

    // Console already writes to the shared store; avoid recording the same failure twice.
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
