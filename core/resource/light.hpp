#pragma once
#include "base/twig.hpp"
#include "math/math.hpp"

namespace mango::resource
{
    enum struct Light_Type : int { directional = 0, point = 1, spot = 2 };

    struct Light : core::Twig<Light>
    {
        Light_Type type = Light_Type::directional;
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 3.5f;

        // For directional light
        math::Vec3 direction{-0.4f, -1.0f, -0.2f};

        // For point/spot light
        float range = 10.0f;

        // For spot light
        float inner_angle = 30.0f;  // degrees
        float outer_angle = 45.0f;  // degrees
    };
}
