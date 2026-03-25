add_executable(stitch main.cpp)
target_link_libraries(stitch PRIVATE gqlxy nlohmann_json::nlohmann_json)
