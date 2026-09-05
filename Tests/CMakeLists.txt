add_executable(
    JanusTests

    Core/ResultTests.cpp
    Core/TimeTests.cpp
    Core/FileSystemTests.cpp
    Core/UUIDTests.cpp
    Core/EventInputTests.cpp
    Core/LogTests.cpp
    Core/CommandTests.cpp
    Core/ReflectionTests.cpp
    MCP/JsonRpcTests.cpp
    MCP/McpProtocolTests.cpp
    MCP/McpMainThreadDispatcherTests.cpp
    MCP/McpPermissionPolicyTests.cpp
    MCP/StdioTransportTests.cpp
    MCP/ToolRegistryTests.cpp
    MCP/ResourceRegistryTests.cpp
    MCP/ReflectionSchemaAdapterTests.cpp
    MCP/SceneResourcesTests.cpp
    MCP/SceneToolsTests.cpp
    Asset/AssetTypesTests.cpp
    Asset/AssetRegistryTests.cpp
    Asset/AssetCacheTests.cpp
    Asset/AssetLoaderTests.cpp
    Asset/AssetServiceTests.cpp
    Asset/LuaScriptAssetTests.cpp
    Platform/WindowConfigTests.cpp
    Application/ApplicationTests.cpp
    Application/ProjectRuntimeTests.cpp
    Editor/EditorActionsTests.cpp
    Editor/EditorCameraTests.cpp
    Editor/EditorLaunchOptionsTests.cpp
    Editor/EditorConsoleTests.cpp
    Editor/InspectorModelTests.cpp
    Editor/McpEditorHostTests.cpp
    Editor/EditorSelectionTests.cpp
    Editor/EditorViewSourceTests.cpp
    Editor/EditorWorkflowTests.cpp
    Editor/EditorWorkspaceLayoutTests.cpp
    Editor/ScenePickerTests.cpp
    Editor/ProjectSessionTests.cpp
    Editor/RuntimeSessionTests.cpp
    Renderer/RendererTypesTests.cpp
    Renderer/RenderQueueTests.cpp
    Renderer/BatchRendererTests.cpp
    Renderer/Renderer2DTests.cpp
    ECS/SparseSetTests.cpp
    ECS/ComponentPoolTests.cpp
    ECS/RegistryTests.cpp
    ECS/ViewTests.cpp
    Scene/SceneIdentityTests.cpp
    Scene/HierarchyTests.cpp
    Scene/SceneCloneTests.cpp
    Scene/TransformTests.cpp
    Scene/SceneSerializationTests.cpp
    Scene/SceneHierarchySerializationTests.cpp
    Scene/SceneReflectionTests.cpp
    Scene/SceneCommandTests.cpp
    Scene/EntityCommandTests.cpp
    Scene/SceneRenderTests.cpp
    Scene/LuaScriptSerializationTests.cpp
    Scripting/LuaVirtualMachineTests.cpp
    Scripting/ScriptEngineTests.cpp
)


target_compile_definitions(
    JanusTests

    PRIVATE

        JANUS_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)


# ============================================================
# Dependencies
# ============================================================

target_link_libraries(
    JanusTests

    PRIVATE

        JanusEngine
        JanusMCP
        JanusEditorCore
        Catch2::Catch2WithMain
)


# ============================================================
# Warnings and CTest Discovery
# ============================================================

janus_set_warnings(JanusTests)

include(Catch)
catch_discover_tests(JanusTests)
