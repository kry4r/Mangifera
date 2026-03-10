#pragma once

#include <cstdint>

namespace mango::graphics
{
    struct Device_Capabilities
    {
        uint32_t graphics_queue_count = 0;
        uint32_t compute_queue_count = 0;
        uint32_t transfer_queue_count = 0;
        bool ray_tracing_supported = false;
        bool dynamic_rendering_supported = false;
        bool timeline_semaphore_supported = false;
        bool descriptor_indexing_supported = false;
    };
}
