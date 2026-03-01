add_executable(fragments main.cpp)
target_link_libraries(fragments PRIVATE ariane::graphql nlohmann_json::nlohmann_json)
