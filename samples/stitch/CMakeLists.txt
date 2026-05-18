add_executable(stitch main.cpp)
target_link_libraries(stitch PRIVATE gqlxy_server::gqlxy_server nlohmann_json::nlohmann_json)
