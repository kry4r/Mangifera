#pragma once
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include "core/base/entity.hpp"

namespace mango::core
{
    static std::size_t node_count = -1;
    struct Scene_Node: public std::enable_shared_from_this<Scene_Node>
    {
        std::size_t id;
        std::string name;
        std::shared_ptr<Scene_Node> parent;
        std::shared_ptr<Scene_Node> next;
        std::shared_ptr<Scene_Node> children;

        Scene_Node()
        {
            node_count ++;
            id = node_count;
            name = "Node" + std::to_string(id);
        }

        Scene_Node(const Scene_Node& other)
            : id(other.id),
            name(other.name),
            parent(other.parent),
            next(other.next),
            children(other.children)
        {
        }

        Scene_Node(Scene_Node&& other) noexcept
            : id(other.id),
            name(std::move(other.name)),
            parent(std::move(other.parent)),
            next(std::move(other.next)),
            children(std::move(other.children))
        {
            other.id = 0;
        }

        Scene_Node& operator=(const Scene_Node& other)
        {
            if (this != &other) {
                id = other.id;
                name = other.name;
                parent = other.parent;
                next = other.next;
                children = other.children;
            }
            return *this;
        }

        Scene_Node& operator=(Scene_Node&& other) noexcept
        {
            if (this != &other) {
                id = other.id;
                name = std::move(other.name);
                parent = std::move(other.parent);
                next = std::move(other.next);
                children = std::move(other.children);

                other.id = 0;
            }
            return *this;
        }
    };

}
