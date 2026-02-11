#include "camera.hpp"

namespace mango::resource
{
    auto Camera::get_view_matrix(const Transform& transform) const -> math::Mat4
    {
        auto forward = transform.rotation * math::Vec3(0, 0, -1);
        auto up = transform.rotation * math::Vec3(0, 1, 0);
        return glm::lookAt(transform.position, transform.position + forward, up);
    }

    auto Camera::get_projection_matrix() const -> math::Mat4
    {
        auto proj = glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
        proj[1][1] *= -1.0f; // Flip Y for Vulkan (Y-down clip space)
        return proj;
    }

    auto Camera::get_view_projection_matrix(const Transform& transform) const -> math::Mat4
    {
        return get_projection_matrix() * get_view_matrix(transform);
    }
}
