add_library(
    JanusMCP
    STATIC
)

target_sources(
    JanusMCP

    PRIVATE

        Host/McpMainThreadDispatcher.cpp
        Host/McpPermissionPolicy.cpp
        Protocol/JsonRpc.cpp
        Protocol/McpProtocol.cpp
        Registry/ToolRegistry.cpp
        Registry/ResourceRegistry.cpp
        Registry/McpCapabilityRouter.cpp
        Resources/SceneResources.cpp
        Schema/ReflectionJsonCodec.cpp
        Schema/ReflectionSchemaAdapter.cpp
        Tools/SceneTools.cpp
        Transport/StdioTransport.cpp

    PUBLIC

        Host/McpMainThreadDispatcher.h
        Host/McpPermissionPolicy.h
        Protocol/JsonRpc.h
        Protocol/McpProtocol.h
        Registry/ToolRegistry.h
        Registry/ResourceRegistry.h
        Registry/McpCapabilityRouter.h
        Resources/SceneResources.h
        Schema/JsonSchema.h
        Schema/ReflectionJsonCodec.h
        Schema/ReflectionSchemaAdapter.h
        Tools/SceneTools.h
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
