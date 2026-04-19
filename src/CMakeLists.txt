if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	message(STATUS "Configuring subproject: ${PROJECT_NAME}")
endif()

string(REPLACE "." ";" _ver_list ${PROJECT_VERSION})
list(GET _ver_list 0 PROJECT_VERSION_MAJOR)

file(GLOB_RECURSE SOURCES_FILES *.cpp)
list(FILTER SOURCES_FILES EXCLUDE REGEX ".*/server/.*")
add_library(gqlxy_server ${SOURCES_FILES})
target_compile_features(gqlxy_server PUBLIC cxx_std_20)
target_include_directories(gqlxy_server
	PUBLIC
		$<BUILD_INTERFACE:${GQLXY_SERVER_ROOT}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${GQLXY_SERVER_ROOT}/src
)
target_link_libraries(
		gqlxy_server
		PUBLIC
		nlohmann_json::nlohmann_json
		gqlxy::core
)

add_library(gqlxy::server ALIAS gqlxy_server)

if(BUILD_STANDALONE_SERVER)
    add_subdirectory(gqlxy/server)
endif()

install(TARGETS gqlxy_server
	EXPORT gqlxy-serverTargets
)

install(EXPORT gqlxy-serverTargets
	FILE gqlxy-serverTargets.cmake
	NAMESPACE gqlxy::
	DESTINATION lib/cmake/gqlxy-server
)

if(EXISTS ${GQLXY_SERVER_ROOT}/include)
	install(DIRECTORY ${GQLXY_SERVER_ROOT}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endif()
