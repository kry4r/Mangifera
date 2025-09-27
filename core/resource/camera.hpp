#include "base/twig.hpp"
#include "math/math.hpp"
namespace mango::resource
{
    struct Camera : core::Twig<Camera>
    {
    public:
        math::Vec3 position {0.0f, 0.0f, 0.0f};
        math::Quat rotation  {1.0f, 0.0f, 0.0f, 0.0f};

        float fov        = 60.0f;
        float aspect     = 16.0f / 9.0f;
        float near_plane = 0.1f;
        float far_plane  = 1000.0f;

        auto get_view_matrix() const -> math::Mat4;

        auto get_projection_matrix() const -> math::Mat4;

        auto get_view_projection_matrix() const -> math::Mat4;

        auto translate(const math::Vec3& delta) -> void;

        auto rotate(const math::Quat& q) -> void;

        auto reset() -> void;
    };
}
