#pragma once
#include <cstdint>

namespace mango::core
{
    struct Entity {
        std::uint32_t id; // 31 index + 1 dirty

        static constexpr std::uint32_t INDEX_MASK = 0xFFFFFFFE; // 高31位
        static constexpr std::uint32_t DIRTY_MASK = 0x00000001; // bit[0]

        std::uint32_t get_index() const {
            return (id & INDEX_MASK) >> 1;
        }

        bool is_dirty() const {
            return id & DIRTY_MASK;
        }

        void set_index(std::uint32_t index) {
            id = (id & ~INDEX_MASK) | ((index & 0x7FFFFFFF) << 1);
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
}
