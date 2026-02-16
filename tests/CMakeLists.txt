if(NOT BUILD_TESTING)
  return()
endif()

find_package(GTest CONFIG REQUIRED)

enable_testing()

file(GLOB TEST_SOURCES "*.cpp")
add_executable(ariane_tests ${TEST_SOURCES})
target_link_libraries(ariane_tests PRIVATE ariane::graphql GTest::gtest_main)
target_include_directories(ariane_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(ariane_tests)

add_test(NAME ariane_tests COMMAND ariane_tests)
