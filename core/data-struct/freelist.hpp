#pragma once
#include "base/entity.hpp"
#include <cstddef>
#include <cstdint>
#include <new>        // Required for placement new and operator new[]/delete[]
#include <vector>
#include <mutex>
#include <cassert>
#include <utility>
#include <type_traits>

namespace mango::core
{
    struct EntityList
    {
    private:
        struct EntityNode
        {
            std::uint32_t next;
        };

        std::vector<Entity> data_;
        std::vector<std::uint32_t> generations_;
        std::uint32_t free_head_;
        std::uint32_t size_;
        static constexpr std::uint32_t INVALID_INDEX = 0xFFFFFFFF;

    public:
        EntityList(): free_head_(INVALID_INDEX), size_(0) {}

        Entity allocate()
        {
            std::uint32_t index;
            if (free_head_ != INVALID_INDEX) {
                index = free_head_;
                free_head_ = reinterpret_cast<EntityNode*>(&data_[index])->next;
            } else {
                index = static_cast<std::uint32_t>(data_.size());
                data_.emplace_back();
                generations_.push_back(0);
            }
            size_++;

            Entity e;
            e.set_index(index);
            e.set_generation(generations_[index]);
            return e;
        }

        template<typename... Args>
        Entity allocate(Args&&... args)
        {
            Entity e = allocate();
            new (&data_[e.get_index()]) Entity(std::forward<Args>(args)...);
            return e;
        }

        bool deallocate(Entity e)
        {
            std::uint32_t index = e.get_index();
            if (index >= data_.size()) return false;

            data_[index].~Entity();
            generations_[index]++; // bump generation
            reinterpret_cast<EntityNode*>(&data_[index])->next = free_head_;
            free_head_ = index;
            size_--;
            return true;
        }

        bool exists(Entity e) const
        {
            std::uint32_t index = e.get_index();
            if (index >= data_.size()) return false;
            return generations_[index] == e.get_generation();
        }

        Entity* get(Entity e)
        {
            return exists(e) ? &data_[e.get_index()] : nullptr;
        }

        auto get_count() -> int
        {
            return size_;
        }
    };

    template<typename T>
    struct Freelist
    {
    private:
        struct FreeNode
        {
            std::uint32_t next;
        };

        std::vector<T> data_;
        std::uint32_t free_head_;
        std::uint32_t size_;
        static constexpr std::uint32_t INVALID_INDEX = 0xFFFFFFFF;

    public:
        Freelist(): free_head_(INVALID_INDEX), size_(0) {}

        std::uint32_t allocate()
        {
            if (free_head_ != INVALID_INDEX) {
                std::uint32_t index = free_head_;
                free_head_ = reinterpret_cast<FreeNode*>(&data_[index])->next;
                size_++;
                return index;
            } else {
                std::uint32_t index = static_cast<std::uint32_t>(data_.size());
                data_.emplace_back();
                size_++;
                return index;
            }
        }

        template<typename... Args>
        std::uint32_t allocate(Args&&... args)
        {
            std::uint32_t index = allocate();
            new (&data_[index]) T(std::forward<Args>(args)...);
            return index;
        }

        void deallocate(std::uint32_t index)
        {
            if (index >= data_.size()) return;

            data_[index].~T();
            reinterpret_cast<FreeNode*>(&data_[index])->next = free_head_;
            free_head_ = index;
            size_--;
        }

        T& get(std::uint32_t index)
        {
            assert(index < data_.size());
            return data_[index];
        }

        const T& get(std::uint32_t index) const
        {
            assert(index < data_.size());
            return data_[index];
        }

        std::uint32_t size() const { return size_; }
        std::uint32_t capacity() const { return static_cast<std::uint32_t>(data_.size()); }
        bool empty() const { return size_ == 0; }

        void clear()
        {
            data_.clear();
            free_head_ = INVALID_INDEX;
            size_ = 0;
        }
    };

}
