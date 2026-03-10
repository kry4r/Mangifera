#include "render_features/passes/sensor_export_pass.hpp"

namespace mango::app
{
    void Sensor_Export_Pass::add_to_graph(Render_Graph& graph) const
    {
        graph.add_pass({"sensor_export", {"scene_depth", "scene_normal", "instance_id_rt", "motion_vector_rt"}, {"sensor_output"}});
    }
}
