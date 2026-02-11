#pragma once
#include "math/math.hpp"
#include "base/entity.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <functional>

namespace mango::physics
{
    // ---- Collision shapes ----

    enum struct Shape_Type : uint8_t
    {
        box,
        sphere,
        capsule,
        convex_mesh,
        triangle_mesh
    };

    struct Box_Shape_Desc
    {
        math::Vec3 half_extents{0.5f, 0.5f, 0.5f};
    };

    struct Sphere_Shape_Desc
    {
        float radius = 0.5f;
    };

    struct Capsule_Shape_Desc
    {
        float radius = 0.5f;
        float half_height = 0.5f;
    };

    using Collision_Shape_Handle = uint64_t;
    constexpr Collision_Shape_Handle INVALID_SHAPE = 0;

    // ---- Rigid body ----

    enum struct Body_Type : uint8_t
    {
        static_body,
        dynamic_body,
        kinematic_body
    };

    struct Physics_Body_Desc
    {
        Body_Type type = Body_Type::dynamic_body;
        Collision_Shape_Handle shape = INVALID_SHAPE;
        float mass = 1.0f;
        float friction = 0.5f;
        float restitution = 0.3f;
        float linear_damping = 0.01f;
        float angular_damping = 0.05f;
        math::Vec3 initial_position{0.0f};
        math::Quat initial_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };

    using Physics_Body_Handle = uint64_t;
    constexpr Physics_Body_Handle INVALID_BODY = 0;

    // ---- Raycast ----

    struct Ray
    {
        math::Vec3 origin;
        math::Vec3 direction;  // Normalized
        float max_distance = 1000.0f;
    };

    struct Raycast_Hit
    {
        math::Vec3 position;
        math::Vec3 normal;
        float distance = 0.0f;
        Physics_Body_Handle body = INVALID_BODY;
        core::Entity entity{0};
    };

    // ---- Contact callback ----

    struct Contact_Info
    {
        Physics_Body_Handle body_a = INVALID_BODY;
        Physics_Body_Handle body_b = INVALID_BODY;
        core::Entity entity_a{0};
        core::Entity entity_b{0};
        math::Vec3 contact_point;
        math::Vec3 contact_normal;
        float penetration_depth = 0.0f;
    };

    using Contact_Callback = std::function<void(const Contact_Info&)>;

    // ---- Abstract physics world ----

    class Physics_World
    {
    public:
        virtual ~Physics_World() = default;

        // Simulation
        virtual void step(float delta_time) = 0;

        // Gravity
        virtual void set_gravity(const math::Vec3& gravity) = 0;
        virtual auto get_gravity() const -> math::Vec3 = 0;

        // Collision shapes
        virtual auto create_box_shape(const Box_Shape_Desc& desc) -> Collision_Shape_Handle = 0;
        virtual auto create_sphere_shape(const Sphere_Shape_Desc& desc) -> Collision_Shape_Handle = 0;
        virtual auto create_capsule_shape(const Capsule_Shape_Desc& desc) -> Collision_Shape_Handle = 0;
        virtual void destroy_shape(Collision_Shape_Handle shape) = 0;

        // Bodies
        virtual auto create_body(const Physics_Body_Desc& desc) -> Physics_Body_Handle = 0;
        virtual void destroy_body(Physics_Body_Handle body) = 0;

        // Body state
        virtual void set_body_position(Physics_Body_Handle body, const math::Vec3& pos) = 0;
        virtual void set_body_rotation(Physics_Body_Handle body, const math::Quat& rot) = 0;
        virtual auto get_body_position(Physics_Body_Handle body) const -> math::Vec3 = 0;
        virtual auto get_body_rotation(Physics_Body_Handle body) const -> math::Quat = 0;

        // Forces
        virtual void add_force(Physics_Body_Handle body, const math::Vec3& force) = 0;
        virtual void add_impulse(Physics_Body_Handle body, const math::Vec3& impulse) = 0;
        virtual void add_torque(Physics_Body_Handle body, const math::Vec3& torque) = 0;
        virtual void set_linear_velocity(Physics_Body_Handle body, const math::Vec3& velocity) = 0;
        virtual auto get_linear_velocity(Physics_Body_Handle body) const -> math::Vec3 = 0;

        // Queries
        virtual auto raycast(const Ray& ray) -> std::optional<Raycast_Hit> = 0;
        virtual auto raycast_all(const Ray& ray) -> std::vector<Raycast_Hit> = 0;

        // Entity binding
        virtual void bind_entity(Physics_Body_Handle body, core::Entity entity) = 0;

        // Contact callbacks
        virtual void set_contact_callback(Contact_Callback callback) = 0;
    };
}
