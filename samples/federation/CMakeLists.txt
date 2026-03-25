add_executable(federation main.cpp)
target_link_libraries(federation PRIVATE gqlxy::server nlohmann_json::nlohmann_json)
