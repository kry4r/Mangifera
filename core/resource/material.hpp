#pragma once
#include "base/twig.hpp"
#include "math/math.hpp"

namespace mango::resource
{
    struct Pbr_Material : core::Twig<Pbr_Material>
    {
        math::Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
        float metallic          = 0.0f;
        float roughness         = 0.5f;
        float ao                = 1.0f;
        float emissive_strength = 0.0f;
        math::Vec3 emissive_color{0.0f, 0.0f, 0.0f};

        // Backward-compatible packed params for push constants
        // x=metallic, y=roughness, z=ao, w=emissive_strength
        math::Vec4 params{0.0f, 0.5f, 1.0f, 0.0f};

        auto sync_params() -> void
        {
            params = {metallic, roughness, ao, emissive_strength};
        }
    };
}
