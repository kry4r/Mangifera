#include "graphics/device.hpp"
#include "tests/test_macros.hpp"

int main()
{
    mango::graphics::Device_Capabilities caps{};
    TEST_ASSERT(caps.graphics_queue_count == 0);
    TEST_ASSERT(!caps.ray_tracing_supported);
    return 0;
}
