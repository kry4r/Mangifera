#pragma once
#include "base/twig.hpp"
#include "math/math.hpp"
#include "transform.hpp"

namespace mango::resource
{
    struct Camera : core::Twig<Camera>
    {
    public:
        float fov        = 60.0f;
        float aspect     = 16.0f / 9.0f;
        float near_plane = 0.1f;
        float far_plane  = 1000.0f;

        auto get_view_matrix(const Transform& transform) const -> math::Mat4;

        auto get_projection_matrix() const -> math::Mat4;

        auto get_view_projection_matrix(const Transform& transform) const -> math::Mat4;
    };
}
