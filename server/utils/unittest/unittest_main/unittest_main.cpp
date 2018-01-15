#include "gtest/gtest.h"
#include <cstdio>

int main(int argc, char* argv[])
{
    puts("Running main() from unittest_main.cpp");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
