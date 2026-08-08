add_executable(code-first main.cpp)
target_link_libraries(code-first PRIVATE gqlxy_server::gqlxy_server nlohmann_json::nlohmann_json)
