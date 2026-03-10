#pragma once

#include <string>
#include "render_core/sensor_output.hpp"

namespace mango::app
{
    struct Render_Targets
    {
        Sensor_Output_Set outputs{};

        auto required(const std::string& name) const -> bool
        {
            if (name == "rgb") return outputs.rgb;
            if (name == "depth") return outputs.depth;
            if (name == "normal") return outputs.normal;
            if (name == "segmentation") return outputs.segmentation;
            if (name == "instance_id") return outputs.instance_id;
            if (name == "motion_vector") return outputs.motion_vector;
            return false;
        }
    };
}
