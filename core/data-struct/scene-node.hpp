#pragma once
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include "base/entity.hpp"

namespace mango::core
{
    struct Scene_Node: public std::enable_shared_from_this<Scene_Node>
    {
        Entity entity;
        std::string name;
        std::shared_ptr<Scene_Node> parent;
        std::shared_ptr<Scene_Node> next;
        std::shared_ptr<Scene_Node> children;

        explicit Scene_Node(Entity e): entity(e)
        {
            name  = "Node" + std::to_string(e.id);
        }

        auto add_child(std::shared_ptr<Scene_Node> child) -> void
        {
            child->parent = shared_from_this();
        }

        auto remove_child(const std::shared_ptr<Scene_Node>& child) -> void
        {

        }

        auto find_child(Entity entity) -> std::shared_ptr<Scene_Node>
        {

        }

        auto get_node_path() -> std::string
        {
            std::string path = "";
            path += name;
            auto node = parent;
            while( node->entity.id != 0)
            {
                path = node->name + "/" + path;
            }
        }
    };
}
