#pragma once

#include "render_core/render_graph.hpp"

namespace mango::app
{
    class Bloom_Pass
    {
    public:
        void add_to_graph(Render_Graph& graph) const;
    };
}
