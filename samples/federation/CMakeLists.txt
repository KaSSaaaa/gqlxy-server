add_executable(federation main.cpp)
target_link_libraries(federation PRIVATE ariane::graphql nlohmann_json::nlohmann_json)
