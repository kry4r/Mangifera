#include "app/render_core/frame_context.hpp"
#include "tests/test_macros.hpp"

int main()
{
    using namespace mango::app;

    Frame_Context ctx{};
    ctx.mode = Run_Mode::headless;
    ctx.outputs.rgb = true;
    ctx.outputs.depth = true;

    TEST_ASSERT(ctx.mode == Run_Mode::headless);
    TEST_ASSERT(ctx.outputs.depth);
    return 0;
}
