add_executable(
    JanusSandbox

    Main.cpp
)

target_link_libraries(
    JanusSandbox

    PRIVATE

        JanusEngine
)

janus_set_warnings(JanusSandbox)