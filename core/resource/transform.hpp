#pragma once
#include "base/twig.hpp"
#include "math/math.hpp"

namespace mango::resource
{
    struct Transform : core::Twig<Transform>
    {
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        math::Vec3 scale{1.0f, 1.0f, 1.0f};

        auto set_position(const math::Vec3& p) -> void;
        auto set_rotation(const math::Quat& r) -> void;
        auto set_scale(const math::Vec3& s) -> void;

        auto get_matrix() const -> math::Mat4;
    };
}
