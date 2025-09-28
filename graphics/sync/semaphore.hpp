#pragma once
#include <memory>
#include <cstdint>

namespace mango::graphics
{
    enum struct Semaphore_Type
    {
        binary,
        timeline,
    };

    struct Semaphore_Desc
    {
        Semaphore_Type type = Semaphore_Type::binary;
        std::uint64_t initial_value = 0; // only valid for timeline semaphore
    };

    // Semaphore abstraction
    struct Semaphore
    {
    public:
        virtual ~Semaphore() = default;

        virtual Semaphore_Type get_type() const = 0;

        //for timeline semaphore
        virtual std::uint64_t get_value() const = 0;
        virtual void signal(std::uint64_t value) = 0;
        virtual void wait(std::uint64_t value) = 0;
    };

    using Semaphore_Handle = std::shared_ptr<Semaphore>;

} // namespace mango::graphics
