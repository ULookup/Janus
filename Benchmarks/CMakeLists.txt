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
