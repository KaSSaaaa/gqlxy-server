add_executable(stitch main.cpp)
target_link_libraries(stitch PRIVATE ariane::graphql nlohmann_json::nlohmann_json)
