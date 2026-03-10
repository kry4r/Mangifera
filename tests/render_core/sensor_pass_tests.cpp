#include "app/render_core/frame_pipeline.hpp"
#include "app/render_core/frame_context.hpp"
#include "tests/test_macros.hpp"

int main()
{
    using namespace mango::app;

    Frame_Context ctx{};
    ctx.outputs.segmentation = true;

    Frame_Pipeline pipeline;
    const auto graph = pipeline.build_graph(ctx);
    const auto order = graph.compile();

    bool found = false;
    for (const auto& pass : order) {
        if (pass == "sensor_export") {
            found = true;
        }
    }

    TEST_ASSERT(found);
    return 0;
}
