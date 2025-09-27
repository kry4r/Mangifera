#pragma once
#include "base/entity.hpp"
#include "base/twig.hpp"
#include "data-struct/freelist.hpp"
#include "data-struct/singleton.hpp"
#include "log/historiographer.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace mango::core
{
    struct Scene_Node;
    struct Scene_Graph;

    struct ITwigStorage
    {
        virtual ~ITwigStorage() = default;
        virtual void remove(Entity e) = 0;
        virtual bool has(Entity e) const = 0;
    };

    template<typename T>
    struct TwigStorage: ITwigStorage
    {
        std::unordered_map<Entity, T> data;

        void insert(Entity e, const T& value)
        {
            data[e] = value;
        }

        T* get(Entity e)
        {
            auto it = data.find(e);
            return (it != data.end()) ? &it->second : nullptr;
        }

        void remove(Entity e) override
        {
            data.erase(e);
        }

        bool has(Entity e) const override
        {
            return data.find(e) != data.end();
        }
    };

    struct World: core::Singleton<World>
    {
    private:
        EntityList entities;
        std::unordered_map<TwigID, std::unique_ptr<ITwigStorage>> twig_stores;
    public:
        World();
        ~World();

        auto is_entity_valid(Entity entity) const -> bool;
        auto get_entities_count() const -> std::size_t;

        auto create_entity() -> Entity;
        auto destroy_entity(Entity entity) -> bool;

        template <typename T>
        auto attach_twig(Entity entity, const T& value) -> void
        {
            if (!is_entity_valid(entity)) {
                UKA_LOG_ERROR_FMT("attach_twig failed: invalid entity {}", entity.id);
                throw std::runtime_error("attach_twig: invalid entity");
            }

            auto id = T::get_static_id();
            auto& store = get_or_create_store<T>(id);
            store.insert(entity, value);
        }

        template <typename T>
        auto detach_twig(Entity entity) -> void
        {
            if (!is_entity_valid(entity)) {
                UKA_LOG_ERROR_FMT("detach_twig failed: invalid entity {}", entity.id);
                throw std::runtime_error("detach_twig: invalid entity");
            }

            auto id = T::get_static_id();
            auto it = twig_stores.find(id);
            if (it == twig_stores.end()) {
                UKA_LOG_ERROR_FMT("detach_twig failed: twig type {} not exist", T::get_static_id());
                throw std::runtime_error("detach_twig: twig type not exist");
            }

            auto& store = *static_cast<TwigStorage<T>*>(it->second.get());
            if (!store.has(entity)) {
                UKA_LOG_ERROR_FMT("detach_twig failed: entity {} does not have this twig", entity.id);
                throw std::runtime_error("detach_twig: entity does not have this twig");
            }
            store.remove(entity);
        }

        template <typename T>
        auto has_twig(Entity entity) -> bool
        {
            if (!is_entity_valid(entity)) {
                UKA_LOG_ERROR_FMT("has_twig failed: invalid entity {}", entity.id);
                throw std::runtime_error("has_twig: invalid entity");
            }

            auto id = T::get_static_id();
            auto it = twig_stores.find(id);
            if (it == twig_stores.end()) {
                UKA_LOG_ERROR_FMT("has_twig: twig type {} not exist", T::get_static_id());
                return false;
            }
            auto& store = *static_cast<TwigStorage<T>*>(it->second.get());
            return store.has(entity);
        }

        template <typename T>
        auto get_twig(Entity entity) -> T&
        {
            if (!is_entity_valid(entity)) {
                UKA_LOG_ERROR_FMT("get_twig failed: invalid entity {}", entity.id);
                throw std::runtime_error("get_twig: invalid entity");
            }

            auto id = T::get_static_id();
            auto it = twig_stores.find(id);
            if (it == twig_stores.end()) {
                UKA_LOG_ERROR_FMT("get_twig failed: twig type {} not exist", T::get_static_id());
                throw std::runtime_error("get_twig: twig type not exist");
            }

            auto& store = *static_cast<TwigStorage<T>*>(it->second.get());
            if (!store.has(entity)) {
                UKA_LOG_ERROR_FMT("get_twig failed: entity {} does not have this twig", entity.id);
                throw std::runtime_error("get_twig: entity does not have this twig");
            }

            return *store.get(entity);
        }

        void clear_all();
    };
}
