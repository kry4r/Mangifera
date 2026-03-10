#pragma once

#include "render_core/render_scene.hpp"

namespace mango::core
{
    struct World;
    struct Scene_Graph;
}

namespace mango::app
{
    class Render_Scene_Extractor
    {
    public:
        static auto extract(const core::World& world, const core::Scene_Graph& graph) -> Render_Scene;
    };
}
