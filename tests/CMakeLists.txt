if(NOT BUILD_TESTING)
  return()
endif()

find_package(GTest CONFIG REQUIRED)

enable_testing()
include(GoogleTest)

file(GLOB UNIT_TEST_SOURCES "unit/*.cpp")

add_executable(ariane_unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(ariane_unit_tests PRIVATE ariane::graphql GTest::gtest_main)
target_include_directories(ariane_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
gtest_discover_tests(ariane_unit_tests)

file(GLOB E2E_TEST_SOURCES "e2e/*.cpp")

add_executable(ariane_e2e_tests ${E2E_TEST_SOURCES})
target_link_libraries(ariane_e2e_tests PRIVATE ariane::graphql GTest::gtest_main)
target_include_directories(ariane_e2e_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(ariane_e2e_tests PRIVATE
    SCHEMA_TODAY_PATH="${CMAKE_SOURCE_DIR}/samples/schema.today.graphql"
    INTROSPECTION_QUERY_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/introspection.graphql"
    INTROSPECTION_RESULT_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/result.json"
)
gtest_discover_tests(ariane_e2e_tests)

file(GLOB INTEGRATION_TEST_SOURCES "integration/*.cpp")

add_executable(ariane_integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(ariane_integration_tests PRIVATE ariane::graphql GTest::gtest_main)
target_include_directories(ariane_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
gtest_discover_tests(ariane_integration_tests)
