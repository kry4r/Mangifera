#include "render_features/passes/post/bloom_pass.hpp"

namespace mango::app
{
    void Bloom_Pass::add_to_graph(Render_Graph& graph) const
    {
        graph.add_pass({"post_process", {"scene_hdr", "scene_depth", "scene_normal"}, {"post_processed"}});
    }
}
