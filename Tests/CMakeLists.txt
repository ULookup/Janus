add_executable(
    JanusTests

    Core/ResultTests.cpp
    Core/TimeTests.cpp
    Core/FileSystemTests.cpp
    Core/UUIDTests.cpp
    Core/EventInputTests.cpp
    Project/ProjectSettingsTests.cpp
    Core/LogTests.cpp
    Core/ProfilerTests.cpp
    Core/LogStoreTests.cpp
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
    Editor/EditorCloseTests.cpp
    Editor/EditorCameraTests.cpp
    Editor/EditorTransformDragTests.cpp
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
    Runtime/RuntimeExecutionTests.cpp
    Game/CombatTests.cpp
    Game/IntegratedTests.cpp
    Animation/AnimationTests.cpp
    Audio/AudioTests.cpp
    Physics/PhysicsTests.cpp
    UI/UILayoutTests.cpp
    UI/UIRenderTests.cpp
    UI/TextTests.cpp
    UI/ButtonTests.cpp
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
    Scene/DuplicateEntityTests.cpp
    Scene/PrefabTests.cpp
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
catch_discover_tests(JanusTests PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")

# ============================================================
# External MCP process E2E
# ============================================================

add_executable(
    JanusMcpExternalHost

    MCP/McpExternalHost.cpp
)

target_include_directories(
    JanusMcpExternalHost

    PRIVATE

        ${CMAKE_CURRENT_SOURCE_DIR}/Renderer
)

target_link_libraries(
    JanusMcpExternalHost

    PRIVATE

        JanusEngine
        JanusMCP
        JanusEditorCore
)

janus_set_warnings(JanusMcpExternalHost)

add_test(
    NAME
        MCP.ExternalStdio.Modern

    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/MCP/mcp_external_e2e.py
        --host
        $<TARGET_FILE:JanusMcpExternalHost>
        --project
        ${CMAKE_SOURCE_DIR}/SandboxProject
        --era
        modern
)

add_test(
    NAME
        MCP.ExternalStdio.Legacy

    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/MCP/mcp_external_e2e.py
        --host
        $<TARGET_FILE:JanusMcpExternalHost>
        --project
        ${CMAKE_SOURCE_DIR}/SandboxProject
        --era
        legacy
)

set_tests_properties(
    MCP.ExternalStdio.Modern
    MCP.ExternalStdio.Legacy

    PROPERTIES

        TIMEOUT 30
        RUN_SERIAL TRUE
)

# The same external host exercises the game observation chain in both protocol eras.
foreach(era IN ITEMS modern legacy)
    add_test(NAME MCP.Integrated.${era}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MCP/integrated_external_e2e.py
            --host $<TARGET_FILE:JanusMcpExternalHost>
            --project ${CMAKE_SOURCE_DIR}/Game --era ${era})
    set_tests_properties(MCP.Integrated.${era} PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_test(NAME MCP.Prefab.${era}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MCP/prefab_external_e2e.py
            --host $<TARGET_FILE:JanusMcpExternalHost>
            --project ${CMAKE_SOURCE_DIR}/Game --era ${era})
    set_tests_properties(MCP.Prefab.${era} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME MCP.Physics.${era}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MCP/physics_external_e2e.py
            --host $<TARGET_FILE:JanusMcpExternalHost>
            --project ${CMAKE_SOURCE_DIR}/Game --era ${era})
    set_tests_properties(MCP.Physics.${era} PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE)
    add_test(NAME MCP.Combat.${era}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/MCP/combat_external_e2e.py
            --host $<TARGET_FILE:JanusMcpExternalHost>
            --project ${CMAKE_SOURCE_DIR}/Game --era ${era})
    set_tests_properties(MCP.Combat.${era} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
endforeach()

# Manual CPU instrumentation overhead measurement; no timing threshold in CTest.
add_executable(JanusCpuProfilerBenchmark Benchmarks/CpuProfilerBenchmark.cpp)
target_link_libraries(JanusCpuProfilerBenchmark PRIVATE JanusEngine)
janus_set_warnings(JanusCpuProfilerBenchmark)

add_executable(JanusGameSystemsBenchmark Benchmarks/GameSystemsBenchmark.cpp)
target_link_libraries(JanusGameSystemsBenchmark PRIVATE JanusEngine JanusEditorCore)
janus_set_warnings(JanusGameSystemsBenchmark)

# Explicit local device smoke; deliberately excluded from device-independent CTest.
add_executable(JanusAudioDeviceSmoke Audio/AudioDeviceSmoke.cpp)
target_link_libraries(JanusAudioDeviceSmoke PRIVATE JanusEngine)
janus_set_warnings(JanusAudioDeviceSmoke)
