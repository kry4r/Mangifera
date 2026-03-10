#pragma once

#include "graphics/capabilities/device-capabilities.hpp"
#include "render_core/frame_context.hpp"
#include "render_core/render_graph.hpp"

namespace mango::app
{
    class Frame_Pipeline
    {
    public:
        auto build_graph(
            const Frame_Context& context,
            const graphics::Device_Capabilities& capabilities = {}) const -> Render_Graph;
    };
}
