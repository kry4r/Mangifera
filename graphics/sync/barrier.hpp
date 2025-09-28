#pragma once
#include <cstdint>

namespace mango::graphics
{
    enum struct Resource_State : std::uint32_t
    {
        undefined = 0,
        common,
        render_target,
        depth_stencil,
        shader_resource,
        unordered_access,
        copy_src,
        copy_dst,
        present,
    };

    struct Barrier
    {
        void* resource = nullptr;
        Resource_State before = Resource_State::undefined;
        Resource_State after  = Resource_State::undefined;
    };

} // namespace mango::graphics
