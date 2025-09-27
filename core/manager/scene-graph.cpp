#include "scene-graph.hpp"
#include <sstream>

namespace mango::core
{
    Scene_Graph::Scene_Graph()
    {
        root = Scene_Node();
        root.id = 0;
        root.name = "Scene";

        current_selected_node = root;
    }

    auto Scene_Graph::get_root_node() -> Scene_Node
    {
        return root;
    }

    auto Scene_Graph::get_current_selected_node() -> Scene_Node
    {
        return current_selected_node;
    }

    auto Scene_Graph::read_write_graph() -> Scene_Graph*
    {
        return current_instance();
    }

    auto Scene_Graph::read_only_graph() -> const Scene_Graph*
    {
        return current_instance();
    }

    auto Scene_Graph::path_of(Scene_Node node) -> std::string
    {
        auto parent = node.parent;
        auto path = node.name;
        while (parent->id != root.id) {
            path = parent->name + "/" + path;
            parent = parent->parent;
        }
        return path;
    }

    auto Scene_Graph::node_of(std::string path) -> std::shared_ptr<Scene_Node>
    {
        std::stringstream ss(path);
        std::string segment;
        std::vector<std::string> parts;
        while (std::getline(ss, segment, '/')) {
            if (!segment.empty())
                parts.push_back(segment);
        }

        auto current = root;
        for (const auto& name : parts) {
            bool found = false;
            auto child = current.children;
            while (child) {
                if (child->name == name) {
                    current = *child.get();
                    found = true;
                    break;
                }
                child = child->next;
            }

            if (!found) {
                return nullptr;
            }
        }

        return std::make_shared<Scene_Node>(current);
    }

    auto Scene_Graph::name_of(Scene_Node node) -> std::string
    {
        return node.name;
    }

    auto Scene_Graph::parent_of(Scene_Node node) -> std::shared_ptr<Scene_Node>
    {
        return node.parent;
    }

    auto Scene_Graph::first_child_of(Scene_Node node) -> std::shared_ptr<Scene_Node>
    {
        return node.children;
    }

    auto Scene_Graph::next_decendent_of(Scene_Node node) ->std::shared_ptr<Scene_Node>
    {
        return node.next;
    }
}
