add_executable(
    JanusTests

    Core/ResultTests.cpp
    Core/TimeTests.cpp
    Core/FileSystemTests.cpp
    Core/EventInputTests.cpp
    Core/LogTests.cpp
    Platform/WindowConfigTests.cpp
    Application/ApplicationTests.cpp
    Renderer/RendererTypesTests.cpp
    Renderer/RenderQueueTests.cpp
    Renderer/BatchRendererTests.cpp
    Renderer/Renderer2DTests.cpp
    ECS/SparseSetTests.cpp
    ECS/ComponentPoolTests.cpp
    ECS/RegistryTests.cpp
    ECS/ViewTests.cpp
    Scene/HierarchyTests.cpp
    Scene/TransformTests.cpp
    Scene/SceneRenderTests.cpp
)


# ============================================================
# Dependencies
# ============================================================

target_link_libraries(
    JanusTests

    PRIVATE

        JanusEngine
        Catch2::Catch2WithMain
)


# ============================================================
# Warnings and CTest Discovery
# ============================================================

janus_set_warnings(JanusTests)

include(Catch)
catch_discover_tests(JanusTests)
