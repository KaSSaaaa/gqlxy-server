add_executable(introspection main.cpp)
target_link_libraries(
    introspection
        PRIVATE
            ariane::graphql
            nlohmann_json::nlohmann_json
)
