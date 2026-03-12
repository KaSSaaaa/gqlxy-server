add_executable(subscription main.cpp)
target_link_libraries(subscription PRIVATE ariane::graphql nlohmann_json::nlohmann_json)
