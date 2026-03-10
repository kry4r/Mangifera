#include "app/render_core/render_graph.hpp"
#include "tests/test_macros.hpp"

int main()
{
    using namespace mango::app;

    Render_Graph graph;
    graph.add_pass({"depth", {}, {"depth_rt"}});
    graph.add_pass({"lighting", {"depth_rt"}, {"hdr_rt"}});

    const auto order = graph.compile();
    TEST_ASSERT(order.size() == 2);
    TEST_ASSERT(order[0] == "depth");
    TEST_ASSERT(order[1] == "lighting");
    return 0;
}
