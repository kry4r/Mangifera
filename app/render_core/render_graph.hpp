#pragma once

#include <string>
#include <vector>
#include "render_core/render_pass_node.hpp"

namespace mango::app
{
    class Render_Graph
    {
    public:
        void add_pass(Render_Pass_Node node);
        auto compile() const -> std::vector<std::string>;

    private:
        std::vector<Render_Pass_Node> passes_;
    };
}
