message(STATUS "GQLXY: standalone server enabled (oatpp ${oatpp_VERSION})")
file(GLOB_RECURSE SERVER_SOURCES *.cpp)

add_library(gqlxy_server ${SERVER_SOURCES})
target_compile_features(gqlxy_server PUBLIC cxx_std_20)
target_include_directories(gqlxy_server
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(gqlxy_server
    PUBLIC
        gqlxy
        oatpp::oatpp
        oatpp::oatpp-websocket
)

add_library(gqlxy::server ALIAS gqlxy_server)

install(TARGETS gqlxy_server
    EXPORT gqlxyTargets
)
