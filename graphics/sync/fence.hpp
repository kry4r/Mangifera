#pragma once
#include <memory>
#include <cstdint>

namespace mango::graphics
{
    struct Fence_Desc
    {
        bool signaled = false;
    };

    // Fence abstraction
    struct Fence
    {
    public:
        virtual ~Fence() = default;

        virtual std::uint64_t get_completed_value() const = 0;

        virtual void wait(std::uint64_t value, std::uint64_t timeout_ns = UINT64_MAX) = 0;

        virtual void signal(std::uint64_t value) = 0;
    };

    using Fence_Handle = std::shared_ptr<Fence>;

} // namespace mango::graphics
