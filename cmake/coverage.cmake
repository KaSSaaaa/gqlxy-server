find_program(GCOVR_EXECUTABLE gcovr REQUIRED)

get_filename_component(_compiler_name "${CMAKE_CXX_COMPILER}" NAME)
string(REGEX REPLACE "^g\\+\\+(-[0-9.]+)?$" "gcov\\1" _gcov_name "${_compiler_name}")
find_program(GCOV_EXECUTABLE NAMES "${_gcov_name}" gcov REQUIRED)

set(COVERAGE_HTML_DIR ${CMAKE_SOURCE_DIR}/coverage)

add_custom_target(coverage
    COMMENT "Generating HTML coverage report -> ${COVERAGE_HTML_DIR}/index.html"
    COMMAND ${CMAKE_CTEST_COMMAND}
        --test-dir ${CMAKE_BINARY_DIR}
        --output-on-failure
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_HTML_DIR}
    COMMAND ${GCOVR_EXECUTABLE}
        --gcov-executable ${GCOV_EXECUTABLE}
        --root ${CMAKE_SOURCE_DIR}
        --filter "${CMAKE_SOURCE_DIR}/src/"
        --filter "${CMAKE_SOURCE_DIR}/include/"
        --html-details ${COVERAGE_HTML_DIR}/index.html
        --html-theme github.dark-blue
        --print-summary
        ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Report: ${COVERAGE_HTML_DIR}/index.html"
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
