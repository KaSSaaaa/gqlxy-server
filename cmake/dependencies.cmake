if(NOT TARGET gqlxy::core)
    include(FetchContent)
    FetchContent_Declare(
        gqlxy-core
        GIT_REPOSITORY https://github.com/KaSSaaaa/gqlxy-core.git
        GIT_TAG        9eb15d7d504bed34e9551776b117e7045a0913df
    )
    FetchContent_MakeAvailable(gqlxy-core)
endif()

find_package(pegtl CONFIG REQUIRED)
find_package(cppgraphqlgen CONFIG COMPONENTS graphqlpeg REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if(BUILD_STANDALONE_SERVER)
    find_package(oatpp CONFIG REQUIRED)
    find_package(oatpp-websocket CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(oatpp-openssl CONFIG REQUIRED)
endif()
