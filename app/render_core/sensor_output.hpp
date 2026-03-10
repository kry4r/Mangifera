#pragma once

namespace mango::app
{
    struct Sensor_Output_Set
    {
        bool rgb = true;
        bool depth = false;
        bool normal = false;
        bool segmentation = false;
        bool instance_id = false;
        bool motion_vector = false;
    };
}
