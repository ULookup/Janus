add_library(
    JanusEditorCore
    STATIC

    EditorActions.cpp
    EditorCamera.cpp
    EditorConsole.cpp
    EditorContext.cpp
    EditorLaunchOptions.cpp
    McpEditorHost.cpp
    InspectorModel.cpp
    EditorViewSource.cpp
    EditorWorkspaceLayout.cpp
    ScenePicker.cpp
    ProjectSession.cpp
    RuntimeSession.cpp
)

target_include_directories(
    JanusEditorCore

    PUBLIC

        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    JanusEditorCore

    PUBLIC

        JanusEngine
        JanusMCP
)

janus_set_warnings(JanusEditorCore)


if(JANUS_BUILD_EDITOR)

    add_library(
        JanusImGui
        STATIC

        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(
        JanusImGui

        SYSTEM PUBLIC

            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(
        JanusImGui

        PRIVATE

            SDL3::SDL3
    )


    add_executable(
        JanusEditor

        Main.cpp
        EditorApplication.cpp
        Panels/AssetBrowserPanel.cpp
        Panels/ConsolePanel.cpp
        Panels/HierarchyPanel.cpp
        Panels/InspectorPanel.cpp
    )

    target_link_libraries(
        JanusEditor

        PRIVATE

            JanusEditorCore
            JanusMCP
            JanusImGui
            SDL3::SDL3
    )

    janus_set_warnings(JanusEditor)

    add_custom_command(
        TARGET JanusEditor
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/SandboxProject
            $<TARGET_FILE_DIR:JanusEditor>/SandboxProject
        COMMENT
            "Copying SandboxProject next to JanusEditor"
    )

endif()
