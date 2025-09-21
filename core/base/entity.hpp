#pragma once
#include <cstdint>

namespace mg::core
{
    struct Entity {
        std::uint32_t id; // 24 index + 4 component-mask bits + 3 version + 1 dirty-mark

        static constexpr std::uint32_t INDEX_MASK = 0xFFFFFF00;
        static constexpr std::uint32_t COMPONENT_MASK = 0x000000F0;
        static constexpr std::uint32_t VERSION_MASK = 0x0000000E;
        static constexpr std::uint32_t DIRTY_MASK = 0x00000001;

        std::uint32_t get_index() const { return (id & INDEX_MASK) >> 8; }
        std::uint32_t get_component_bits() const { return (id & COMPONENT_MASK) >> 4; }
        std::uint32_t get_version() const { return (id & VERSION_MASK) >> 1; }
        bool is_dirty() const { return id & DIRTY_MASK; }

        void set_index(std::uint32_t index) {
            id = (id & ~INDEX_MASK) | ((index & 0xFFFFFF) << 8);
        }
        void set_component_bits(std::uint32_t bits) {
            id = (id & ~COMPONENT_MASK) | ((bits & 0xF) << 4);
        }
        void set_version(std::uint32_t version) {
            id = (id & ~VERSION_MASK) | ((version & 0x7) << 1);
        }
        void set_dirty(bool dirty) {
            id = dirty ? (id | DIRTY_MASK) : (id & ~DIRTY_MASK);
        }

        bool operator==(const Entity& other) const { return id == other.id; }
        bool operator!=(const Entity& other) const { return id != other.id; }
    };
}

#include <functional>
namespace std {
    template<>
    struct hash<mg::core::Entity> {
        std::size_t operator()(const mg::core::Entity& e) const noexcept {
            return std::hash<std::uint32_t>()(e.id);
        }
    };
}
