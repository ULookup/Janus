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
        Core/Time/FrameClock.cpp
        Core/Input/InputState.cpp
        Core/Math/Mat4.cpp

        Renderer/RendererStatistics.cpp
        Renderer/RenderDevice.h
        Renderer/RenderQueue.h
        Renderer/RenderQueue.cpp
        Renderer/BatchRenderer.h
        Renderer/BatchRenderer.cpp

        Platform/SDL/SDLPlatform.cpp
        Platform/SDL/SDLOpenGLContext.cpp
        Platform/SDL/SDLOpenGLContext.h

        Platform/Window/WindowConfig.cpp
        Platform/SDL/SDLWindow.cpp

        Application/Application.cpp
        Application/Detail/ApplicationDependencies.h

    PUBLIC

        Core/Base.h
        Core/Types.h
        Core/Assert.h

        Core/Error/Error.h
        Core/Error/Result.h

        Core/Log/Log.h

        Core/FileSystem/FileSystem.h

        Core/Time/FrameClock.h
        Core/Time/TimeStep.h

        Core/Event/Event.h
        Core/Input/InputState.h

        Core/Math/Vector2.h
        Core/Math/Mat4.h

        Renderer/RendererTypes.h
        Renderer/Sprite.h
        Renderer/OrthographicCamera.h
        Renderer/RendererStatistics.h

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


# ============================================================
# Dependencies
# ============================================================

target_link_libraries(
    JanusEngine

    PRIVATE

        SDL3::SDL3
        glad_gl_45
        glm::glm

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
