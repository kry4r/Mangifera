#pragma once
#include "freelist.hpp"
#include "twig.hpp"
#include "base/entity.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <type_traits>
#include <any>

namespace mango::core
{
    struct Grove
    {
    private:
        std::size_t id;
        TwigSet twigs;
        std::vector<std::byte> data;
    public:
        auto get_grove_id() -> std::size_t { return id;}
    };

    struct Chunk
    {
    private:
        std::unordered_map<Entity,Grove> grove_entity_mapping;
    public:
        inline auto get_enity_grove(Entity e) -> Grove
        {
            return grove_entity_mapping[e];
        }
    };
}
