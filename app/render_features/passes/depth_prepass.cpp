#include "render_features/passes/depth_prepass.hpp"

namespace mango::app
{
    void Depth_Prepass_Pass::add_to_graph(Render_Graph& graph) const
    {
        graph.add_pass({"pre_render", {}, {"shadow_data", "depth_rt"}});
    }
}
