add_executable(
    JanusRendererBenchmark
    Renderer2DBenchmark.cpp
)

target_link_libraries(
    JanusRendererBenchmark
    PRIVATE
        JanusEngine
)

janus_set_warnings(JanusRendererBenchmark)

add_executable(
    JanusECSBenchmark
    ECSBenchmark.cpp
)

target_link_libraries(
    JanusECSBenchmark
    PRIVATE
        JanusEngine
)

janus_set_warnings(JanusECSBenchmark)
