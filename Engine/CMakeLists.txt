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

        Platform/SDL/SDLPlatform.cpp

        Platform/SDL/SDLWindow.cpp

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

        Platform/Platform.h
        Platform/Window/WindowConfig.h
        Platform/Window/Window.h
        Platform/SDL/SDLWindow.h
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
