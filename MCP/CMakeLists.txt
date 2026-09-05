add_library(
    JanusMCP
    STATIC
)

target_sources(
    JanusMCP

    PRIVATE

        Protocol/JsonRpc.cpp
        Protocol/McpProtocol.cpp
        Transport/StdioTransport.cpp

    PUBLIC

        Protocol/JsonRpc.h
        Protocol/McpProtocol.h
        Transport/StdioTransport.h
)

target_include_directories(
    JanusMCP

    PUBLIC

        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    JanusMCP

    PUBLIC

        JanusEngine
        nlohmann_json::nlohmann_json
)

janus_set_warnings(JanusMCP)
