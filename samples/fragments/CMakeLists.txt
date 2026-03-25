add_executable(fragments main.cpp)
target_link_libraries(fragments PRIVATE gqlxy nlohmann_json::nlohmann_json)
