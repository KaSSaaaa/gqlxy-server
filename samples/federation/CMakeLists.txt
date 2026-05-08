add_executable(federation main.cpp)
target_link_libraries(federation PRIVATE gqlxy_server::gqlxy_server nlohmann_json::nlohmann_json)
