#pragma once

#include <cstdint>
#include <vector>

namespace mango::app
{
    struct Render_Mesh_Instance
    {
        uint64_t entity_id = 0;
        uint32_t material_slot = 0;
    };

    struct Render_Point_Light
    {
        uint64_t entity_id = 0;
        float intensity = 0.0f;
    };

    struct Render_Scene
    {
        std::vector<Render_Mesh_Instance> mesh_instances;
        std::vector<Render_Point_Light> point_lights;
    };
}
