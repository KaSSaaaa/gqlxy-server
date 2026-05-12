if(NOT TARGET standalone)
    message(STATUS "Skipping mcp-server: standalone-server feature not enabled")
    return()
endif()

add_executable(mcp-server main.cpp)
target_link_libraries(mcp-server PRIVATE standalone nlohmann_json::nlohmann_json)
target_compile_definitions(mcp-server PRIVATE
    SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/schema.graphql")
