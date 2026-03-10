#include "render_core/render_scene_extractor.hpp"
#include "core/manager/world.hpp"
#include "core/manager/scene-graph.hpp"

namespace mango::app
{
    auto Render_Scene_Extractor::extract(const core::World& world, const core::Scene_Graph& graph) -> Render_Scene
    {
        (void)world;
        (void)graph;
        return {};
    }
}
