#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string_view>
#include <typeinfo>
#include <type_traits>
#include <algorithm>
#include <bitset>
#include <cassert>
#include "base/entity.hpp"
#include "ns/archetype.hpp"
#include "ns/freelist.hpp"
#include "ns/scene-node.hpp"

namespace mg::core
{

    class WholeManager
    {
    private:
        Freelist<Entity> entity_freelist_;
        std::unordered_map<Entity, std::uint32_t> entity_to_archetype_index_;
        std::unordered_map<Entity, Twig_Sign> entity_signatures_;

        std::shared_ptr<SceneNode> scene_root_;
        std::unordered_map<Entity, std::shared_ptr<SceneNode>> entity_to_node_;

        std::uint32_t next_entity_version_;

    public:
        WholeManager(): next_entity_version_(1)
        {
            Entity root_entity;
            root_entity.id = 0;
            scene_root_ = std::make_shared<SceneNode>(root_entity);
        }

        Entity create_entity()
        {
            std::uint32_t index = entity_freelist_.allocate();
            Entity entity;
            entity.set_index(index);
            entity.set_version(next_entity_version_++);
            entity.set_component_bits(0);
            entity.set_dirty(false);

            auto node = std::make_shared<SceneNode>(entity);
            entity_to_node_[entity] = node;
            scene_root_->add_child(node);

            return entity;
        }

        void destroy_entity(Entity entity)
        {
            auto sig_it = entity_signatures_.find(entity);

            entity_freelist_.deallocate(entity.get_index());
        }

        template<typename T, typename... Args>
        T* add_component(Entity entity, Args&&... args)
        {

        }

        template<typename T>
        T* get_component(Entity entity)
        {
            static_assert(std::is_base_of_v<Twig, T>, "T must derive from Twig");

            auto sig_it = entity_signatures_.find(entity);
            return nullptr;
        }

        template<typename T>
        bool has_component(Entity entity)
        {
            return get_component<T>(entity) != nullptr;
        }

        template<typename T>
        void remove_component(Entity entity)
        {

        }

        void set_parent(Entity child, Entity parent)
        {
            auto child_it = entity_to_node_.find(child);
            auto parent_it = entity_to_node_.find(parent);

            if (child_it != entity_to_node_.end() && parent_it != entity_to_node_.end()) {
                auto child_node = child_it->second;
                auto parent_node = parent_it->second;

                if (auto current_parent = child_node->parent.lock()) {
                    current_parent->remove_child(child_node);
                }

            }
        }

        std::vector<Entity> get_children(Entity parent)
        {
            auto it = entity_to_node_.find(parent);
            if (it != entity_to_node_.end()) {
                std::vector<Entity> result;
                for (const auto& child : it->second->children) {
                    result.push_back(child->entity);
                }
                return result;
            }
            return {};
        }

        Entity get_parent(Entity entity)
        {
            auto it = entity_to_node_.find(entity);
            if (it != entity_to_node_.end()) {
                if (auto parent = it->second->parent.lock()) {
                    return parent->entity;
                }
            }
            return Entity{0};
        }

        template<typename... Components>
        std::vector<Entity> get_entities_with()
        {
            Twig_Sign signature;
            ((signature.set(get_component_index<Components>())), ...);

            std::vector<Entity> result;
            return result;
        }

        std::shared_ptr<SceneNode> get_scene_root() const
        {
            return scene_root_;
        }

    private:
        void move_entity_to_archetype(Entity entity, Twig_Sign new_signature)
        {

        }
    };
} // namespace mg::core
