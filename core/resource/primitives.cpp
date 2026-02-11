#include "primitives.hpp"
#include <cmath>

namespace mango::resource
{
    auto create_plane_mesh(float size, uint32_t subdivisions) -> Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;

        float half = size * 0.5f;
        float step = size / static_cast<float>(subdivisions);

        for (uint32_t z = 0; z <= subdivisions; ++z) {
            for (uint32_t x = 0; x <= subdivisions; ++x) {
                Vertex v{};
                v.position = {
                    -half + static_cast<float>(x) * step,
                    0.0f,
                    -half + static_cast<float>(z) * step
                };
                v.normal = {0.0f, 1.0f, 0.0f};
                v.uv = {
                    static_cast<float>(x) / static_cast<float>(subdivisions),
                    static_cast<float>(z) / static_cast<float>(subdivisions)
                };
                vertices.push_back(v);
            }
        }

        uint32_t row = subdivisions + 1;
        for (uint32_t z = 0; z < subdivisions; ++z) {
            for (uint32_t x = 0; x < subdivisions; ++x) {
                uint32_t i0 = z * row + x;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + row;
                uint32_t i3 = i2 + 1;
                indices.insert(indices.end(), {i0, i2, i1, i1, i2, i3});
            }
        }

        Mesh mesh;
        mesh.set_vertices(vertices);
        mesh.set_indices(indices);
        return mesh;
    }

    auto create_cube_mesh(float h) -> Mesh
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;

        auto face = [&](math::Vec3 n, math::Vec3 right, math::Vec3 up) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            math::Vec3 center = n * h;
            math::Vec3 r = right * h;
            math::Vec3 u = up * h;

            vertices.push_back({center - r - u, n, {0, 0}});
            vertices.push_back({center + r - u, n, {1, 0}});
            vertices.push_back({center + r + u, n, {1, 1}});
            vertices.push_back({center - r + u, n, {0, 1}});

            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        };

        // +Y (top)
        face({0, 1, 0}, {1, 0, 0}, {0, 0, -1});
        // -Y (bottom)
        face({0, -1, 0}, {1, 0, 0}, {0, 0, 1});
        // +X (right)
        face({1, 0, 0}, {0, 0, -1}, {0, 1, 0});
        // -X (left)
        face({-1, 0, 0}, {0, 0, 1}, {0, 1, 0});
        // +Z (front)
        face({0, 0, 1}, {1, 0, 0}, {0, 1, 0});
        // -Z (back)
        face({0, 0, -1}, {-1, 0, 0}, {0, 1, 0});

        Mesh mesh;
        mesh.set_vertices(vertices);
        mesh.set_indices(indices);
        return mesh;
    }

    auto create_sphere_mesh(float radius, uint32_t slices, uint32_t stacks) -> Mesh
    {
        const float PI = 3.14159265358979323846f;
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;

        for (uint32_t j = 0; j <= stacks; ++j) {
            float phi = PI * static_cast<float>(j) / static_cast<float>(stacks);
            float sin_phi = std::sin(phi);
            float cos_phi = std::cos(phi);

            for (uint32_t i = 0; i <= slices; ++i) {
                float theta = 2.0f * PI * static_cast<float>(i) / static_cast<float>(slices);
                float sin_theta = std::sin(theta);
                float cos_theta = std::cos(theta);

                Vertex v{};
                v.normal = {sin_phi * cos_theta, cos_phi, sin_phi * sin_theta};
                v.position = {v.normal.x * radius, v.normal.y * radius, v.normal.z * radius};
                v.uv = {
                    static_cast<float>(i) / static_cast<float>(slices),
                    static_cast<float>(j) / static_cast<float>(stacks)
                };
                vertices.push_back(v);
            }
        }

        uint32_t row = slices + 1;
        for (uint32_t j = 0; j < stacks; ++j) {
            for (uint32_t i = 0; i < slices; ++i) {
                uint32_t i0 = j * row + i;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + row;
                uint32_t i3 = i2 + 1;
                indices.insert(indices.end(), {i0, i2, i1, i1, i2, i3});
            }
        }

        Mesh mesh;
        mesh.set_vertices(vertices);
        mesh.set_indices(indices);
        return mesh;
    }
}
