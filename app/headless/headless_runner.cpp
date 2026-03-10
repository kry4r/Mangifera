#include "headless_runner.hpp"

#include "log/historiographer.hpp"

namespace mango::app
{
    int Headless_Runner::run(const Headless_Run_Options& options)
    {
        UH_INFO_FMT("Running headless bootstrap for {} frame(s)", options.frames);
        UH_INFO_FMT("Headless outputs: rgb={}, depth={}", options.export_rgb, options.export_depth);

        for (uint32_t frame_index = 0; frame_index < options.frames; ++frame_index) {
            (void)frame_index;
        }

        return 0;
    }
}
