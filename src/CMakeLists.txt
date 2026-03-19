if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	message(STATUS "Configuring subproject: ${PROJECT_NAME}")
endif()

string(REPLACE "." ";" _ver_list ${PROJECT_VERSION})
list(GET _ver_list 0 PROJECT_VERSION_MAJOR)

file(GLOB_RECURSE SOURCES_FILES *.cpp)
list(FILTER SOURCES_FILES EXCLUDE REGEX ".*/server/.*")
add_library(ariane_graphql ${SOURCES_FILES})
target_compile_features(ariane_graphql PUBLIC cxx_std_20)
target_include_directories(ariane_graphql
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(
		ariane_graphql
		PUBLIC
		cppgraphqlgen::graphqlpeg
		nlohmann_json::nlohmann_json
)

add_library(ariane::graphql ALIAS ariane_graphql)

if(BUILD_STANDALONE_SERVER)
    add_subdirectory(ariane/server)
endif()

install(TARGETS ariane_graphql
	EXPORT ariane_graphqlTargets
)

install(EXPORT ariane_graphqlTargets
	FILE ariane_graphqlTargets.cmake
	NAMESPACE ariane::
	DESTINATION lib/cmake/ariane_graphql
)

if(EXISTS ${CMAKE_SOURCE_DIR}/include)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endif()
