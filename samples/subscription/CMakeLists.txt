add_executable(subscription main.cpp)
target_link_libraries(subscription PRIVATE gqlxy::server nlohmann_json::nlohmann_json)
