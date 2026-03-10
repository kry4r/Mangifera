#include "graphics/capabilities/device-capabilities.hpp"
#include "tests/test_macros.hpp"

int main()
{
    mango::graphics::Device_Capabilities caps{};
    TEST_ASSERT(caps.graphics_queue_count == 0);
    TEST_ASSERT(caps.compute_queue_count == 0);
    TEST_ASSERT(caps.transfer_queue_count == 0);
    TEST_ASSERT(!caps.ray_tracing_supported);
    TEST_ASSERT(!caps.dynamic_rendering_supported);
    return 0;
}
