add_executable(
    JanusTests

    Core/ResultTests.cpp
    Core/TimeTests.cpp
    Core/FileSystemTests.cpp
    Core/EventInputTests.cpp
    Platform/WindowConfigTests.cpp
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
