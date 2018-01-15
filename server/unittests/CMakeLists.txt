cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

add_custom_target(unittests)
set_target_properties(unittests PROPERTIES FOLDER "tests")

function(add_unittest test_suite test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name}
        gtest
        unittest_main
        ${TEST_LINK_COMPONENTS}
    )
    
    # Avoid putting the tests in Debug/ or Release/ with MSVC
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set_target_properties(${test_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif ()
    
    # Add unit test definition
    set_target_properties(${test_name} PROPERTIES
        COMPILE_DEFINITIONS "UNIT_TEST_BUILD;GTEST_HAS_STD_TUPLE_;\
            GTEST_HAS_TR1_TUPLE=0"
    )
    
    add_dependencies(${test_suite} ${test_name})
    
    include_directories(${CMAKE_SOURCE_DIR}/utils/unittest/gtest/include)
    
    set_property(GLOBAL APPEND PROPERTY unit_test_commands
        ${CMAKE_CURRENT_BINARY_DIR}/${test_name}${CMAKE_EXECUTABLE_SUFFIX})
endfunction()

function(add_unittest_helper test_name)
    add_unittest(unittests ${test_name} ${ARGN})
endfunction()

# tests
add_subdirectory(framework)

# custom targets to run tests:

# TODO: add option to run specific tests, i.e. make check-*

get_property(testbins GLOBAL PROPERTY unit_test_commands)
add_custom_command(
    OUTPUT invoke_py_cmd
    COMMENT "Running all unit tests"
    COMMAND python \"${CMAKE_SOURCE_DIR}/utils/unittest/run_tests.py\"
        \"${CLANG_RPATH}\" ${testbins}
    # DEPENDS unittests -- don't rebuild tests; do what with
    # "make unittests" or just "make"
)

# check-all: target to run all tests
add_custom_target(check-all
    DEPENDS invoke_py_cmd
)
