cmake_minimum_required(VERSION 3.10.0)
option(ENABLE_VCPKG_BOOTSTRAP "Bootstrap vcpkg in the build dir and install manifest deps" OFF)
option(BUILD_STANDALONE_SERVER "Build the standalone HTTP/WebSocket/SSE GraphQL server (requires oatpp)" OFF)

# Prefer an existing vcpkg in PATH; otherwise clone+bootstrap into the preset build dir.
find_program(SYSTEM_VCPKG_EXECUTABLE NAMES vcpkg vcpkg.exe)

if(ENABLE_VCPKG_BOOTSTRAP)
	# Default vcpkg root when bootstrapping locally into the build dir.
	set(VCPKG_ROOT "${CMAKE_BINARY_DIR}/vcpkg")

	if(SYSTEM_VCPKG_EXECUTABLE)
		# If a system vcpkg binary exists, use its installation directory as
		# the vcpkg root. Passing an empty (new) directory via
		# --vcpkg-root causes vcpkg to look for triplets in that empty tree
		# and fail with "Invalid triplet". Use the system vcpkg's root and
		# only set the install root for where packages are deployed.
		get_filename_component(SYSTEM_VCPKG_DIR "${SYSTEM_VCPKG_EXECUTABLE}" DIRECTORY)
		set(VCPKG_ROOT "${SYSTEM_VCPKG_DIR}")
		message(STATUS "Found system vcpkg: ${SYSTEM_VCPKG_EXECUTABLE}; using system vcpkg root ${VCPKG_ROOT}")

		set(_vcpkg_feature_args "")
		if(VCPKG_MANIFEST_FEATURES)
			foreach(_feature ${VCPKG_MANIFEST_FEATURES})
				list(APPEND _vcpkg_feature_args "--x-feature" "${_feature}")
			endforeach()
		endif()

		execute_process(COMMAND "${SYSTEM_VCPKG_EXECUTABLE}" "install" "--triplet" "${VCPKG_TRIPLET}" "--x-install-root" "${CMAKE_BINARY_DIR}/vcpkg_installed" ${_vcpkg_feature_args}
			RESULT_VARIABLE _vcpkg_install_rc
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	else()
		message(FATAL_ERROR "vcpkg not found in PATH. Install vcpkg and ensure it's on PATH, or run configure with ENABLE_VCPKG_BOOTSTRAP=OFF to skip bootstrap.")
	endif()

	# Set a cache variable so the presets or other consumers can use it as the toolchain file
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE PATH "vcpkg toolchain file" FORCE)
endif()

project(ariane-graphql-server VERSION 0.1.0 LANGUAGES C CXX)
include(CTest)
enable_testing()

include(cmake/dependencies.cmake)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(src)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(samples)
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
	${CMAKE_SOURCE_DIR}/cmake/ariane_graphql-config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/ariane_graphqlConfig.cmake
	INSTALL_DESTINATION lib/cmake/ariane_graphql
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/ariane_graphqlConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/ariane_graphqlConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/ariane_graphqlConfigVersion.cmake
	DESTINATION lib/cmake/ariane_graphql)