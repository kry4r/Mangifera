#include "mesh.hpp"

namespace mango::resource
{
    auto Mesh::set_vertices(const std::vector<Vertex>& verts) -> void {
        vertices = verts;
    }

    auto Mesh::set_indices(const std::vector<std::uint32_t>& inds) -> void {
        indices = inds;
    }

    auto Mesh::get_vertices() const -> const std::vector<Vertex>& {
        return vertices;
    }

    auto Mesh::get_indices() const -> const std::vector<std::uint32_t>& {
        return indices;
    }

    auto Mesh::get_vertex_count() const -> std::size_t {
        return vertices.size();
    }

    auto Mesh::get_index_count() const -> std::size_t {
        return indices.size();
    }
}
