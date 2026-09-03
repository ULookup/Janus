add_executable(
    JanusSandbox

    Main.cpp
)

target_link_libraries(
    JanusSandbox

    PRIVATE

        JanusEngine
)

add_custom_command(
    TARGET JanusSandbox
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/SandboxProject"
        "$<TARGET_FILE_DIR:JanusSandbox>/SandboxProject"
    COMMENT "Copying disk-backed SandboxProject next to JanusSandbox"
)

janus_set_warnings(JanusSandbox)
