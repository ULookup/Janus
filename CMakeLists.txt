cmake_minimum_required(VERSION 3.24)

project(
    Janus
    VERSION 0.1.0
    LANGUAGES CXX
)


# ============================================================
# Global Configuration
# ============================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# ============================================================
# Options
# ============================================================

option(
    JANUS_BUILD_SANDBOX
    "Build Janus Sandbox"
    ON
)

option(
    JANUS_BUILD_TESTS
    "Build Janus tests"
    OFF
)


# ============================================================
# Common Project Configuration
# ============================================================

function(janus_set_warnings TARGET_NAME)

    if(MSVC)

        target_compile_options(
            ${TARGET_NAME}

            PRIVATE

                /W4
                /permissive-
        )

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")

        target_compile_options(
            ${TARGET_NAME}

            PRIVATE

                -Wall
                -Wextra
                -Wpedantic
        )

    endif()

endfunction()


# ============================================================
# Dependencies
# ============================================================

include(FetchContent)

set(SPDLOG_SYSTEM_INCLUDES ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    spdlog

    GIT_REPOSITORY
        https://github.com/gabime/spdlog.git

    GIT_TAG
        v1.15.3
)

FetchContent_MakeAvailable(spdlog)

# ------------------------------------------------------------
# SDL3
# ------------------------------------------------------------

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3

    GIT_REPOSITORY
        https://github.com/libsdl-org/SDL.git

    GIT_TAG
        release-3.4.14

    GIT_SHALLOW
        TRUE
)

FetchContent_MakeAvailable(SDL3)


# ============================================================
# Engine
# ============================================================

add_subdirectory(Engine)


# ============================================================
# Sandbox
# ============================================================

if(JANUS_BUILD_SANDBOX)
    add_subdirectory(Sandbox)
endif()


# ============================================================
# Tests
# ============================================================

if(JANUS_BUILD_TESTS)

    FetchContent_Declare(
        Catch2

        GIT_REPOSITORY
            https://github.com/catchorg/Catch2.git

        GIT_TAG
            v3.15.3

        GIT_SHALLOW
            TRUE
    )

    FetchContent_MakeAvailable(Catch2)

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

    enable_testing()

    add_subdirectory(Tests)

endif()
