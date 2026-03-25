add_executable(stitch main.cpp)
target_link_libraries(stitch PRIVATE gqlxy::server nlohmann_json::nlohmann_json)
