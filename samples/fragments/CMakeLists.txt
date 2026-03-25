add_executable(fragments main.cpp)
target_link_libraries(fragments PRIVATE gqlxy::server nlohmann_json::nlohmann_json)
