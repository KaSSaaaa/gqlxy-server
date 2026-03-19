message(STATUS "Ariane: standalone server enabled (oatpp ${oatpp_VERSION})")
file(GLOB_RECURSE SERVER_SOURCES *.cpp)

add_library(ariane_graphql_server ${SERVER_SOURCES})
target_compile_features(ariane_graphql_server PUBLIC cxx_std_20)
target_include_directories(ariane_graphql_server
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ariane_graphql_server
    PUBLIC
        ariane::graphql
        oatpp::oatpp
        oatpp::oatpp-websocket
)

add_library(ariane::graphql::server ALIAS ariane_graphql_server)

install(TARGETS ariane_graphql_server
    EXPORT ariane_graphqlTargets
)
