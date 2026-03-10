#include "app/render_core/render_targets.hpp"
#include "tests/test_macros.hpp"

int main()
{
    using namespace mango::app;

    Render_Targets targets{};
    TEST_ASSERT(targets.required("rgb"));
    TEST_ASSERT(!targets.required("segmentation"));
    return 0;
}
