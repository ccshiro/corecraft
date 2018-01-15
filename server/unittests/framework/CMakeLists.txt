include_directories(
    ${CMAKE_SOURCE_DIR}/src/framework/
    ${CMAKE_SOURCE_DIR}/src/framework/Utilities
)

set(FRAMEWORKTEST_SRC
    grid.cpp
    grid_helpers.cpp
    ref_counter.cpp
)

add_unittest_helper(framework-test ${FRAMEWORKTEST_SRC})
