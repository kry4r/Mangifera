#pragma once
#include <bitset>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>
#include "base/entity.hpp"
#include "base/twig.hpp"
namespace mg::core
{
    class Archetype
    {
    private:
        Twig_Sign signature_;
        std::vector<Entity> entities_;


    public:
        std::unordered_map<std::size_t, std::unique_ptr<std::vector<std::unique_ptr<Twig>>>> components_;
        explicit Archetype(Twig_Sign signature): signature_(signature) {}

        Twig_Sign get_signature() const { return signature_; }

        std::uint32_t add_entity(Entity entity)
        {
            std::uint32_t index = static_cast<std::uint32_t>(entities_.size());
            entities_.push_back(entity);

            for (std::size_t i = 0; i < signature_.size(); ++i) {
                if (signature_[i]) {
                    if (components_.find(i) == components_.end()) {
                        components_[i] = std::make_unique<std::vector<std::unique_ptr<Twig>>>();
                    }
                    components_[i]->emplace_back(nullptr);
                }
            }

            return index;
        }

        Entity remove_entity(std::uint32_t index)
        {
            if (index >= entities_.size()) {
                return Entity{0};
            }

            Entity moved_entity{0};

            if (index < entities_.size() - 1) {
                moved_entity = entities_.back();

                entities_[index] = entities_.back();

                for (auto& [type_id, comp_vec] : components_) {
                    auto& comp = (*comp_vec);
                    if (index < comp.size() && !comp.empty()) {
                        comp[index] = std::move(comp.back());
                    }
                }
            }

            entities_.pop_back();
            for (auto& [type_id, comp_vec] : components_) {
                if (!comp_vec->empty()) {
                    comp_vec->pop_back();
                }
            }

            return moved_entity;
        }

        template<typename T>
        void set_component(std::uint32_t index, std::unique_ptr<T> component)
        {
            static_assert(std::is_base_of_v<Twig, T>, "T must derive from Twig");
            std::size_t type_index = get_component_index<T>();

            if (index < entities_.size() && signature_[type_index]) {
                (*components_[type_index])[index] = std::move(component);
            }
        }

        template<typename T>
        T* get_component(std::uint32_t index)
        {
            static_assert(std::is_base_of_v<Twig, T>, "T must derive from Twig");
            std::size_t type_index = get_component_index<T>();

            if (index < entities_.size() && signature_[type_index]) {
                auto it = components_.find(type_index);
                if (it != components_.end() && index < it->second->size()) {
                    return static_cast<T*>((*it->second)[index].get());
                }
            }
            return nullptr;
        }

        const std::vector<Entity>& get_entities() const { return entities_; }
        std::size_t size() const { return entities_.size(); }
        bool empty() const { return entities_.empty(); }
    };
}
