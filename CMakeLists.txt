cmake_minimum_required(VERSION 3.24)

project(
    Janus
    VERSION 0.1.0
    LANGUAGES C CXX
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

option(
    JANUS_BUILD_BENCHMARKS
    "Build Janus benchmarks"
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


# ------------------------------------------------------------
# glad
# ------------------------------------------------------------

find_package(Python3 REQUIRED COMPONENTS Interpreter)

FetchContent_Declare(
    glad

    GIT_REPOSITORY
        https://github.com/Dav1dde/glad.git

    GIT_TAG
        v2.0.8

    SOURCE_SUBDIR
        cmake
)

FetchContent_MakeAvailable(glad)

glad_add_library(
    glad_gl_45
    STATIC
    REPRODUCIBLE
    LOADER
    API gl:core=4.5
)


# ------------------------------------------------------------
# glm
# ------------------------------------------------------------

FetchContent_Declare(
    glm

    GIT_REPOSITORY
        https://github.com/g-truc/glm.git

    GIT_TAG
        1.0.3
)

FetchContent_MakeAvailable(glm)


# ------------------------------------------------------------
# nlohmann/json
# ------------------------------------------------------------

FetchContent_Declare(
    nlohmann_json

    GIT_REPOSITORY
        https://github.com/nlohmann/json.git

    GIT_TAG
        v3.11.3

    GIT_SHALLOW
        TRUE
)

FetchContent_MakeAvailable(nlohmann_json)


# ------------------------------------------------------------
# stb
# ------------------------------------------------------------

FetchContent_Declare(
    stb

    GIT_REPOSITORY
        https://github.com/nothings/stb.git

    GIT_TAG
        2c980bb59875b0d32144a71867fbdebb2f77cd20
)

FetchContent_MakeAvailable(stb)


# ------------------------------------------------------------
# Lua
# ------------------------------------------------------------

FetchContent_Declare(
    lua

    URL
        https://www.lua.org/ftp/lua-5.4.9.tar.gz

    URL_HASH
        SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6

    DOWNLOAD_EXTRACT_TIMESTAMP
        TRUE
)

FetchContent_MakeAvailable(lua)

add_library(
    JanusLua
    STATIC

    ${lua_SOURCE_DIR}/src/lapi.c
    ${lua_SOURCE_DIR}/src/lauxlib.c
    ${lua_SOURCE_DIR}/src/lbaselib.c
    ${lua_SOURCE_DIR}/src/lcode.c
    ${lua_SOURCE_DIR}/src/lctype.c
    ${lua_SOURCE_DIR}/src/ldebug.c
    ${lua_SOURCE_DIR}/src/ldo.c
    ${lua_SOURCE_DIR}/src/ldump.c
    ${lua_SOURCE_DIR}/src/lfunc.c
    ${lua_SOURCE_DIR}/src/lgc.c
    ${lua_SOURCE_DIR}/src/llex.c
    ${lua_SOURCE_DIR}/src/lmathlib.c
    ${lua_SOURCE_DIR}/src/lmem.c
    ${lua_SOURCE_DIR}/src/lobject.c
    ${lua_SOURCE_DIR}/src/lopcodes.c
    ${lua_SOURCE_DIR}/src/lparser.c
    ${lua_SOURCE_DIR}/src/lstate.c
    ${lua_SOURCE_DIR}/src/lstring.c
    ${lua_SOURCE_DIR}/src/lstrlib.c
    ${lua_SOURCE_DIR}/src/ltable.c
    ${lua_SOURCE_DIR}/src/ltablib.c
    ${lua_SOURCE_DIR}/src/ltm.c
    ${lua_SOURCE_DIR}/src/lundump.c
    ${lua_SOURCE_DIR}/src/lutf8lib.c
    ${lua_SOURCE_DIR}/src/lvm.c
    ${lua_SOURCE_DIR}/src/lzio.c
)

set_target_properties(
    JanusLua
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
)

target_include_directories(
    JanusLua

    SYSTEM PUBLIC

        ${lua_SOURCE_DIR}/src
)

target_compile_definitions(
    JanusLua

    PRIVATE

        $<$<CONFIG:Debug>:LUA_USE_APICHECK>
        $<$<PLATFORM_ID:Windows>:LUA_USE_WINDOWS>
)

if(UNIX)
    target_link_libraries(JanusLua PRIVATE m)
endif()


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
# Benchmarks
# ============================================================

if(JANUS_BUILD_BENCHMARKS)
    add_subdirectory(Benchmarks)
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
