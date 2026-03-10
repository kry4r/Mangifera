#include "app/headless/headless_runner.hpp"
#include "tests/test_macros.hpp"

int main()
{
    mango::app::Headless_Run_Options options{};
    options.frames = 4;
    TEST_ASSERT(options.frames == 4);
    return 0;
}
