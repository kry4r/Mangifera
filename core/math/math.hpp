#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mango::math {
    using Vec2  = glm::vec2;
    using Vec3  = glm::vec3;
    using Vec4  = glm::vec4;
    using Mat3  = glm::mat3;
    using Mat4  = glm::mat4;
    using Quat  = glm::quat;

    inline auto identity_mat4() -> Mat4 {
        return Mat4(1.0f);
    }

    inline auto translate(const Mat4& m, const Vec3& v) -> Mat4 {
        return glm::translate(m, v);
    }

    inline auto scale(const Mat4& m, const Vec3& v) -> Mat4 {
        return glm::scale(m, v);
    }

    inline auto rotate(const Mat4& m, float angle_rad, const Vec3& axis) -> Mat4 {
        return glm::rotate(m, angle_rad, axis);
    }

    inline auto to_mat4(const Quat& q) -> Mat4 {
        return glm::mat4_cast(q);
    }

    inline auto look_at(const Vec3& eye, const Vec3& center, const Vec3& up) -> Mat4 {
        return glm::lookAt(eye, center, up);
    }

    inline auto perspective(float fovy_rad, float aspect, float z_near, float z_far) -> Mat4 {
        return glm::perspective(fovy_rad, aspect, z_near, z_far);
    }

    inline auto ortho(float left, float right, float bottom, float top, float z_near, float z_far) -> Mat4 {
        return glm::ortho(left, right, bottom, top, z_near, z_far);
    }

    inline auto normalize(const Vec3& v) -> Vec3 {
        return glm::normalize(v);
    }

    inline auto cross(const Vec3& a, const Vec3& b) -> Vec3 {
        return glm::cross(a, b);
    }

    inline auto dot(const Vec3& a, const Vec3& b) -> float {
        return glm::dot(a, b);
    }
}
