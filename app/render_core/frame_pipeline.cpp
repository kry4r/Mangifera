#include "render_core/frame_pipeline.hpp"

#include "render_features/passes/depth_prepass.hpp"
#include "render_features/passes/post/bloom_pass.hpp"
#include "render_features/passes/post/tonemap_pass.hpp"
#include "render_features/passes/sensor_export_pass.hpp"

namespace mango::app
{
    auto Frame_Pipeline::build_graph(
        const Frame_Context& context,
        const graphics::Device_Capabilities& capabilities) const -> Render_Graph
    {
        Render_Graph graph;

        Depth_Prepass_Pass depth_prepass{};
        Bloom_Pass bloom{};
        Tonemap_Pass tonemap{};
        Sensor_Export_Pass sensor_export{};

        depth_prepass.add_to_graph(graph);
        graph.add_pass({"scene_render", {"shadow_data", "depth_rt"}, {"scene_hdr", "scene_depth", "scene_normal", "instance_id_rt", "motion_vector_rt"}});

        if (capabilities.ray_tracing_supported) {
            graph.add_pass({"rt_reflections", {"scene_depth", "scene_normal", "scene_hdr"}, {"reflection_rt"}});
        }

        bloom.add_to_graph(graph);

        if (context.outputs.depth
            || context.outputs.normal
            || context.outputs.segmentation
            || context.outputs.instance_id
            || context.outputs.motion_vector) {
            sensor_export.add_to_graph(graph);
        }

        if (context.outputs.rgb) {
            tonemap.add_to_graph(graph);
        }

        graph.add_pass({"imgui", {"swapchain"}, {"present"}});
        return graph;
    }
}
