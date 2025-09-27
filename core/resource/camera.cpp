#include "camera.hpp"

namespace mango::resource
{
    auto Camera::get_view_matrix() const -> math::Mat4
    {
        auto forward = rotation * math::Vec3(0, 0, -1);
        auto up = rotation * math::Vec3(0, 1, 0);
        return glm::lookAt(position, position + forward, up);
    }

    auto Camera::get_projection_matrix() const -> math::Mat4
    {
        return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
    }

    auto Camera::get_view_projection_matrix() const -> math::Mat4
    {
        return get_projection_matrix() * get_view_matrix();
    }

    auto Camera::translate(const math::Vec3& delta) -> void
    {
        position += delta;
    }

    auto Camera::rotate(const math::Quat& q) -> void
    {
        rotation = glm::normalize(q * rotation);
    }

    auto Camera::reset() -> void
    {
        position = {0, 0, 0};
        rotation = {1, 0, 0, 0};
        fov = 60.0f;
        aspect = 16.0f / 9.0f;
        near_plane = 0.1f;
        far_plane = 1000.0f;
    }
}
