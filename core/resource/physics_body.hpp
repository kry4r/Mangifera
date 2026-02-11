#pragma once
#include "base/twig.hpp"
#include "physics/physics_world.hpp"

namespace mango::resource
{
    struct Physics_Body : core::Twig<Physics_Body>
    {
        physics::Body_Type type = physics::Body_Type::dynamic_body;
        physics::Shape_Type shape_type = physics::Shape_Type::box;

        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.3f;
        float linear_damping = 0.01f;
        float angular_damping = 0.05f;

        // Shape parameters (interpreted based on shape_type)
        math::Vec3 shape_half_extents{0.5f, 0.5f, 0.5f};  // box
        float shape_radius = 0.5f;                          // sphere, capsule
        float shape_half_height = 0.5f;                     // capsule

        // Runtime handle (set by physics integration, not serialized)
        physics::Physics_Body_Handle runtime_handle = physics::INVALID_BODY;
        physics::Collision_Shape_Handle runtime_shape = physics::INVALID_SHAPE;
    };
}
