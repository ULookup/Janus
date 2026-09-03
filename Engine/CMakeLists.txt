add_library(
    JanusEngine
    STATIC
)


# ============================================================
# Sources
# ============================================================

target_sources(
    JanusEngine

    PRIVATE

        Core/Log/Log.cpp
        Core/FileSystem/FileSystem.cpp
        Core/UUID/UUID.cpp
        Core/Time/FrameClock.cpp
        Core/Input/InputState.cpp
        Core/Math/Mat4.cpp

        Asset/AssetMetadata.cpp
        Asset/AssetRegistry.cpp
        Asset/AssetCache.h
        Asset/AssetCache.cpp
        Asset/AssetService.cpp
        Asset/Loader/TextureLoader.h
        Asset/Loader/TextureLoader.cpp
        Asset/Loader/ShaderSourceLoader.h
        Asset/Loader/ShaderSourceLoader.cpp

        ECS/SparseSet.cpp
        ECS/Registry.cpp

        Scene/Scene.cpp
        Scene/SceneSerializer.cpp
        Scene/SceneDeserializer.cpp
        Scene/SceneRenderer.cpp

        Scripting/LuaStackGuard.h
        Scripting/LuaVirtualMachine.h
        Scripting/LuaVirtualMachine.cpp

        Renderer/RendererStatistics.cpp
        Renderer/RenderDevice.h
        Renderer/RenderQueue.h
        Renderer/RenderQueue.cpp
        Renderer/BatchRenderer.h
        Renderer/BatchRenderer.cpp
        Renderer/Renderer2D.h
        Renderer/Renderer2D.cpp

        Platform/SDL/SDLPlatform.cpp
        Platform/SDL/SDLOpenGLContext.cpp
        Platform/SDL/SDLOpenGLContext.h

        Platform/Window/WindowConfig.cpp
        Platform/SDL/SDLWindow.cpp

        Application/Application.cpp
        Application/Detail/ApplicationDependencies.h

        Backend/OpenGL/OpenGLRenderDevice.h
        Backend/OpenGL/OpenGLRenderDevice.cpp

    PUBLIC

        Core/Base.h
        Core/Types.h
        Core/Assert.h

        Core/Error/Error.h
        Core/Error/Result.h

        Core/Log/Log.h

        Core/FileSystem/FileSystem.h
        Core/UUID/UUID.h

        Core/Time/FrameClock.h
        Core/Time/TimeStep.h

        Core/Event/Event.h
        Core/Input/InputState.h

        Core/Math/Vector2.h
        Core/Math/Mat4.h

        Asset/AssetHandle.h
        Asset/AssetMetadata.h
        Asset/AssetRegistry.h
        Asset/AssetService.h

        ECS/Entity.h
        ECS/SparseSet.h
        ECS/ComponentPool.h
        ECS/View.h
        ECS/Registry.h

        Scene/Hierarchy.h
        Scene/Components.h
        Scene/SceneMetadata.h
        Scene/Scene.h
        Scene/SceneSerializer.h
        Scene/SceneDeserializer.h
        Scene/SceneRenderer.h

        Renderer/RendererTypes.h
        Renderer/Sprite.h
        Renderer/OrthographicCamera.h
        Renderer/RendererStatistics.h
        Renderer/Renderer2D.h

        Platform/Platform.h
        Platform/Graphics/GraphicsContext.h
        Platform/Window/WindowConfig.h
        Platform/Window/Window.h
        Platform/SDL/SDLWindow.h

        Application/ApplicationConfig.h
        Application/ApplicationClient.h
        Application/Application.h
)


# ============================================================
# Include Directories
# ============================================================

target_include_directories(
    JanusEngine

    PUBLIC

        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(
    JanusEngine

    SYSTEM PRIVATE

        ${stb_SOURCE_DIR}
)


# ============================================================
# Dependencies
# ============================================================

target_link_libraries(
    JanusEngine

    PRIVATE

        SDL3::SDL3
        glad_gl_45
        glm::glm
        nlohmann_json::nlohmann_json
        JanusLua

    PUBLIC

        spdlog::spdlog
)


# ============================================================
# Build Configuration
# ============================================================

target_compile_definitions(
    JanusEngine

    PUBLIC

        $<$<CONFIG:Debug>:JANUS_DEBUG>
        $<$<CONFIG:Release>:JANUS_RELEASE>
        $<$<CONFIG:RelWithDebInfo>:JANUS_RELEASE>
        $<$<CONFIG:MinSizeRel>:JANUS_RELEASE>
)


# ============================================================
# Warnings
# ============================================================

janus_set_warnings(JanusEngine)
