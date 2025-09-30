#pragma once
#include <cstdint>
#include <string>
#include <memory>

namespace mango::graphics
{
    enum struct Buffer_Type
    {
        vertex,
        index,
        uniform,
        storage,
    };

    enum struct Memory_Type
    {
        gpu_only,
        cpu_only,
        cpu2gpu,
        gpu2cpu,
    };

    struct Buffer_Desc
    {
        std::size_t size = 0;
        Buffer_Type usage = Buffer_Type::vertex;
        Memory_Type memory = Memory_Type::gpu_only;
    };

    struct Buffer
    {
        virtual ~Buffer() = default;
        virtual auto get_buffer_desc() const -> const Buffer_Desc& = 0;
    };

    using Buffer_Handle = std::shared_ptr<Buffer>;
}
