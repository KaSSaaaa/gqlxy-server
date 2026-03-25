add_executable(introspection main.cpp)
target_link_libraries(
    introspection
        PRIVATE
            gqlxy::server
            nlohmann_json::nlohmann_json
)
target_compile_definitions(introspection PRIVATE
    SCHEMA_TODAY_PATH="${CMAKE_SOURCE_DIR}/samples/schema.today.graphql"
    INTROSPECTION_QUERY_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/introspection.graphql"
    RESULT_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/result.json"
    OUTPUT_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/output.json"
)
