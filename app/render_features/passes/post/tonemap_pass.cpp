#include "render_features/passes/post/tonemap_pass.hpp"

namespace mango::app
{
    void Tonemap_Pass::add_to_graph(Render_Graph& graph) const
    {
        graph.add_pass({"final_blit", {"post_processed"}, {"swapchain"}});
    }
}
