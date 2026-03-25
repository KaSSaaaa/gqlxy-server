add_executable(directives main.cpp)
target_link_libraries(directives PRIVATE gqlxy::server)
target_compile_features(directives PRIVATE cxx_std_20)
