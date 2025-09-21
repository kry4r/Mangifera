#pragma once
#include <memory>
#include <vector>
#include <algorithm>
#include "base/entity.hpp"

namespace mg::core
{
    struct SceneNode: public std::enable_shared_from_this<SceneNode>
    {
        Entity entity;
        std::weak_ptr<SceneNode> parent;
        std::vector<std::shared_ptr<SceneNode>> children;

        explicit SceneNode(Entity e): entity(e) {}

        void add_child(std::shared_ptr<SceneNode> child)
        {
            child->parent = shared_from_this();
            children.push_back(child);
        }

        void remove_child(const std::shared_ptr<SceneNode>& child)
        {
            auto it = std::find(children.begin(), children.end(), child);
            if (it != children.end()) {
                (*it)->parent.reset();
                children.erase(it);
            }
        }

        std::shared_ptr<SceneNode> find_child(Entity entity)
        {
            for (auto& child : children) {
                if (child->entity == entity) {
                    return child;
                }
                auto found = child->find_child(entity);
                if (found) return found;
            }
            return nullptr;
        }
    };
}
