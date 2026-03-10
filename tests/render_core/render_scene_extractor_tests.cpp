#include "app/render_core/render_scene_extractor.hpp"
#include "core/manager/world.hpp"
#include "core/manager/scene-graph.hpp"
#include "tests/test_macros.hpp"

int main()
{
    auto& world = *mango::core::World::current_instance();
    auto& graph = *mango::core::Scene_Graph::current_instance();

    const auto scene = mango::app::Render_Scene_Extractor::extract(world, graph);
    TEST_ASSERT(scene.mesh_instances.empty());
    TEST_ASSERT(scene.point_lights.empty());

    mango::core::Scene_Graph::destroy_instance();
    mango::core::World::destroy_instance();
    return 0;
}
