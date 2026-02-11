#include "scene-graph.hpp"
#include <sstream>

namespace mango::core
{
    Scene_Graph::Scene_Graph()
    {
        root = std::make_shared<Scene_Node>();
        root->id = 0;
        root->name = "Scene";
        root->parent = nullptr;
        root->children = nullptr;
        root->next = nullptr;

        current_selected_node = root;
    }

    auto Scene_Graph::get_root_node() -> std::shared_ptr<Scene_Node>
    {
        return root;
    }

    auto Scene_Graph::get_current_selected_node() -> std::shared_ptr<Scene_Node>
    {
        return current_selected_node;
    }

    auto Scene_Graph::set_current_selected_node(std::shared_ptr<Scene_Node> node) -> void
    {
        if (node) {
            current_selected_node = node;
        }
    }

    auto Scene_Graph::read_write_graph() -> Scene_Graph*
    {
        return current_instance();
    }

    auto Scene_Graph::read_only_graph() -> const Scene_Graph*
    {
        return current_instance();
    }

    auto Scene_Graph::path_of(std::shared_ptr<Scene_Node> node) -> std::string
    {
        if (!node) {
            return "";
        }

        std::vector<std::string> parts;
        auto current = node;
        while (current) {
            parts.push_back(current->name);
            if (current == root) {
                break;
            }
            current = current->parent;
        }

        std::string path;
        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
            if (!path.empty()) {
                path += "/";
            }
            path += *it;
        }
        return path;
    }

    auto Scene_Graph::node_of(const std::string& path) -> std::shared_ptr<Scene_Node>
    {
        std::stringstream ss(path);
        std::string segment;
        std::vector<std::string> parts;
        while (std::getline(ss, segment, '/')) {
            if (!segment.empty())
                parts.push_back(segment);
        }

        if (parts.empty()) {
            return root;
        }

        auto current = root;
        size_t index = 0;
        if (!parts.empty() && parts.front() == root->name) {
            index = 1;
        }

        for (; index < parts.size(); ++index) {
            const auto& name = parts[index];
            bool found = false;
            auto child = current->children;
            while (child) {
                if (child->name == name) {
                    current = child;
                    found = true;
                    break;
                }
                child = child->next;
            }

            if (!found) {
                return nullptr;
            }
        }

        return current;
    }

    auto Scene_Graph::name_of(std::shared_ptr<Scene_Node> node) -> std::string
    {
        return node ? node->name : "";
    }

    auto Scene_Graph::parent_of(std::shared_ptr<Scene_Node> node) -> std::shared_ptr<Scene_Node>
    {
        return node ? node->parent : nullptr;
    }

    auto Scene_Graph::first_child_of(std::shared_ptr<Scene_Node> node) -> std::shared_ptr<Scene_Node>
    {
        return node ? node->children : nullptr;
    }

    auto Scene_Graph::next_decendent_of(std::shared_ptr<Scene_Node> node) -> std::shared_ptr<Scene_Node>
    {
        return node ? node->next : nullptr;
    }

    auto Scene_Graph::add_entity_to_scene(Entity entity, std::shared_ptr<Scene_Node> parent, const std::string& name) -> std::shared_ptr<Scene_Node>
    {
        if (!parent) {
            parent = root;
        }

        auto new_node = std::make_shared<Scene_Node>();
        new_node->name = name.empty() ? ("Entity " + std::to_string(entity.id)) : name;
        new_node->parent = parent;

        if (parent->children) {
            auto current = parent->children;
            while (current->next) {
                current = current->next;
            }
            current->next = new_node;
        } else {
            parent->children = new_node;
        }

        entities_mapping.push_back({entity, new_node});
        return new_node;
    }
}
