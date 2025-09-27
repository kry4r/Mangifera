#pragma once
#include <cstdint>

namespace mango::core
{
    struct Entity {
        std::uint32_t id;

        static constexpr std::uint32_t INDEX_MASK      = 0x7FFFFE;     // bits [1..23]
        static constexpr std::uint32_t DIRTY_MASK      = 0x000001;     // bit 0
        static constexpr std::uint32_t GENERATION_MASK = 0xFF000000;   // bits [24..31]

        explicit constexpr Entity(std::uint32_t id_value = 0) : id(id_value) {}

        std::uint32_t get_index() const {
            return (id & INDEX_MASK) >> 1;
        }

        std::uint32_t get_generation() const {
            return (id & GENERATION_MASK) >> 24;
        }

        bool is_dirty() const {
            return id & DIRTY_MASK;
        }

        void set_index(std::uint32_t index) {
            id = (id & ~INDEX_MASK) | ((index & 0x7FFFFF) << 1);
        }

        void set_generation(std::uint32_t gen) {
            id = (id & ~GENERATION_MASK) | ((gen & 0xFF) << 24);
        }

        void set_dirty(bool dirty) {
            id = dirty ? (id | DIRTY_MASK) : (id & ~DIRTY_MASK);
        }

        bool operator==(const Entity& other) const {
            return id == other.id;
        }
        bool operator!=(const Entity& other) const {
            return id != other.id;
        }
    };

    constexpr Entity INVALID_ENTITY = Entity{0xFFFFFFFF};

}
