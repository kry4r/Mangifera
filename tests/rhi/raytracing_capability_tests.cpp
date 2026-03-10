#include "app/render_core/frame_pipeline.hpp"
#include "app/render_core/frame_context.hpp"
#include "graphics/capabilities/device-capabilities.hpp"
#include "tests/test_macros.hpp"

int main()
{
    using namespace mango;

    app::Frame_Context context{};
    app::Frame_Pipeline pipeline;
    graphics::Device_Capabilities caps{};

    const auto raster_graph = pipeline.build_graph(context, caps);
    const auto raster_order = raster_graph.compile();
    bool found_rt_reflections = false;
    for (const auto& pass : raster_order) {
        if (pass == "rt_reflections") {
            found_rt_reflections = true;
        }
    }
    TEST_ASSERT(!found_rt_reflections);

    caps.ray_tracing_supported = true;
    const auto hybrid_graph = pipeline.build_graph(context, caps);
    const auto hybrid_order = hybrid_graph.compile();
    found_rt_reflections = false;
    for (const auto& pass : hybrid_order) {
        if (pass == "rt_reflections") {
            found_rt_reflections = true;
        }
    }
    TEST_ASSERT(found_rt_reflections);
    return 0;
}
