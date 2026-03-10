#pragma once

#include <cstdlib>
#include <iostream>

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "TEST_ASSERT failed: " #expr << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)
