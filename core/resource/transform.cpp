#include "transform.hpp"

namespace mango::resource
{
    auto Transform::set_position(const math::Vec3& p) -> void { position = p; }
    auto Transform::set_rotation(const math::Quat& r) -> void { rotation = r; }
    auto Transform::set_scale(const math::Vec3& s) -> void { scale = s; }

    auto Transform::get_matrix() const -> math::Mat4
    {
        math::Mat4 T = math::translate(math::Mat4(1.0f), position);
        math::Mat4 R = math::to_mat4(rotation);
        math::Mat4 S = math::scale(math::Mat4(1.0f), scale);
        return T * R * S;
    }
}
