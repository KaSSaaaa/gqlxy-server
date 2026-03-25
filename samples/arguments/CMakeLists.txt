add_executable(arguments main.cpp)
target_link_libraries(arguments PRIVATE gqlxy::server nlohmann_json::nlohmann_json)
target_compile_definitions(arguments PRIVATE
    SCHEMA_TODAY_PATH="${CMAKE_SOURCE_DIR}/samples/schema.today.graphql")