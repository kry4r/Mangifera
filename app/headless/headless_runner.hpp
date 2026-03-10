#pragma once

#include <cstdint>

namespace mango::app
{
    struct Headless_Run_Options
    {
        uint32_t frames = 1;
        bool export_rgb = true;
        bool export_depth = false;
    };

    class Headless_Runner
    {
    public:
        int run(const Headless_Run_Options& options);
    };
}
