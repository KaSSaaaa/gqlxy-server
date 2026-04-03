message(STATUS "GQLXY: standalone server enabled (oatpp ${oatpp_VERSION})")
file(GLOB_RECURSE SERVER_SOURCES *.cpp)

add_library(gqlxy_standalone_server ${SERVER_SOURCES})
target_compile_features(gqlxy_standalone_server PUBLIC cxx_std_20)
target_include_directories(gqlxy_standalone_server
    PUBLIC
        $<BUILD_INTERFACE:${GQLXY_SERVER_ROOT}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(gqlxy_standalone_server
    PUBLIC
        gqlxy_server
        oatpp::oatpp
        oatpp::oatpp-websocket
)

add_library(gqlxy::standalone_server ALIAS gqlxy_standalone_server)

install(TARGETS gqlxy_standalone_server
    EXPORT gqlxy-serverTargets
)
