add_executable(subscription main.cpp)
target_link_libraries(subscription PRIVATE gqlxy_server::gqlxy_server nlohmann_json::nlohmann_json)
