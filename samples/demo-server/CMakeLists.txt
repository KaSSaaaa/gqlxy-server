if(NOT TARGET standalone)
    message(STATUS "Skipping demo-server: standalone-server feature not enabled")
    return()
endif()

add_executable(demo-server main.cpp)
target_link_libraries(demo-server PRIVATE standalone nlohmann_json::nlohmann_json)
target_compile_definitions(demo-server PRIVATE
    SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/schema.graphql")
