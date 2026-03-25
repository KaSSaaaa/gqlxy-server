if(NOT BUILD_TESTING)
  return()
endif()

find_package(GTest CONFIG REQUIRED)
find_package(boost_beast CONFIG REQUIRED)

enable_testing()
include(GoogleTest)

file(GLOB UNIT_TEST_SOURCES "unit/*.cpp")

add_executable(gqlxy_unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(gqlxy_unit_tests PRIVATE gqlxy GTest::gtest_main)
target_include_directories(gqlxy_unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
gtest_discover_tests(gqlxy_unit_tests)

file(GLOB E2E_TEST_SOURCES "e2e/*.cpp")

add_executable(gqlxy_e2e_tests ${E2E_TEST_SOURCES})
target_link_libraries(gqlxy_e2e_tests PRIVATE gqlxy GTest::gtest_main)
target_include_directories(gqlxy_e2e_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(gqlxy_e2e_tests PRIVATE
    SCHEMA_TODAY_PATH="${CMAKE_SOURCE_DIR}/samples/schema.today.graphql"
    INTROSPECTION_QUERY_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/introspection.graphql"
    INTROSPECTION_RESULT_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/result.json"
    DEMO_SERVER_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/samples/demo-server/schema.graphql"
)
gtest_discover_tests(gqlxy_e2e_tests)

file(GLOB INTEGRATION_TEST_SOURCES "integration/*.cpp")

add_executable(gqlxy_integration_tests ${INTEGRATION_TEST_SOURCES})
target_link_libraries(gqlxy_integration_tests PRIVATE gqlxy GTest::gtest_main)
target_include_directories(gqlxy_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
gtest_discover_tests(gqlxy_integration_tests)

if(TARGET gqlxy_server)
    file(GLOB SERVER_E2E_TEST_SOURCES "server/*.cpp")
    add_executable(gqlxy_server_e2e_tests ${SERVER_E2E_TEST_SOURCES})
    target_link_libraries(gqlxy_server_e2e_tests PRIVATE gqlxy::server GTest::gtest_main Boost::beast)
    target_compile_definitions(gqlxy_server_e2e_tests PRIVATE
        DEMO_SERVER_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/samples/demo-server/schema.graphql"
        INTROSPECTION_QUERY_PATH="${CMAKE_SOURCE_DIR}/samples/introspection/introspection.graphql"
    )
    gtest_discover_tests(gqlxy_server_e2e_tests
        PROPERTIES RUN_SERIAL TRUE
    )
endif()
