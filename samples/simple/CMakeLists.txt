add_executable(simple main.cpp)
target_link_libraries(simple PRIVATE gqlxy nlohmann_json::nlohmann_json)
target_compile_definitions(simple PRIVATE
    SCHEMA_TODAY_PATH="${CMAKE_SOURCE_DIR}/samples/schema.today.graphql")