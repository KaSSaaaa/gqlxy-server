add_executable(fragments main.cpp)
target_link_libraries(fragments PRIVATE gqlxy_server::gqlxy_server nlohmann_json::nlohmann_json)
