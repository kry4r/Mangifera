#pragma once

#include <cstdint>
#include "render_core/run_mode.hpp"
#include "render_core/sensor_output.hpp"

namespace mango::app
{
    struct Frame_Context
    {
        uint64_t frame_index = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        Run_Mode mode = Run_Mode::runtime;
        Sensor_Output_Set outputs{};
    };
}
