include_directories(
    ${CMAKE_SOURCE_DIR}/utils/unittest/gtest/include
)

set(SOURCE_CODE
    unittest_main
)

add_library(${SOURCE_CODE}
    unittest_main.cpp
)

target_link_libraries(unittest_main
    gtest
)

# Add unit test definition
set_target_properties(unittest_main PROPERTIES
    COMPILE_DEFINITIONS UNIT_TEST_BUILD
)
