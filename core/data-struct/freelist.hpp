#pragma once
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
    template<typename T>
    class Freelist
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
