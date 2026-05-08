message(STATUS "GQLXY: standalone server enabled (oatpp ${oatpp_VERSION})")
file(GLOB_RECURSE SERVER_SOURCES *.cpp)

add_library(standalone ${SERVER_SOURCES})
target_compile_features(standalone PUBLIC cxx_std_20)
target_include_directories(standalone
    PUBLIC
        $<BUILD_INTERFACE:${GQLXY_SERVER_ROOT}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${GQLXY_SERVER_ROOT}/src
)
target_link_libraries(standalone
    PUBLIC
        gqlxy_server
        oatpp::oatpp
        oatpp::oatpp-websocket
        oatpp::oatpp-openssl
)

add_library(gqlxy_server::standalone ALIAS standalone)

install(TARGETS standalone
    EXPORT gqlxy-serverTargets
)
