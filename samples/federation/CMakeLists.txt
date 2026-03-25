add_executable(federation main.cpp)
target_link_libraries(federation PRIVATE gqlxy nlohmann_json::nlohmann_json)
