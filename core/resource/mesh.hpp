#pragma once
#include "core/base/twig.hpp"
#include "core/math/math.hpp"
#include <vector>



namespace mango::resource
{
    struct Vertex {
        mango::math::Vec3 position;
        mango::math::Vec3 normal;
        mango::math::Vec2 uv;
    };

    struct Mesh : core::Twig<Mesh>
    {
    public:
        auto set_vertices(const std::vector<Vertex>& verts) -> void;

        auto set_indices(const std::vector<std::uint32_t>& inds) -> void;

        auto get_vertices() const -> const std::vector<Vertex>&;

        auto get_indices() const -> const std::vector<std::uint32_t>&;

        auto get_vertex_count() const -> std::size_t;

        auto get_index_count() const -> std::size_t;

    private:
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };
}
