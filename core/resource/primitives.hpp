#pragma once
#include "mesh.hpp"

namespace mango::resource
{
    auto create_plane_mesh(float size = 10.0f, uint32_t subdivisions = 1) -> Mesh;
    auto create_cube_mesh(float half_extent = 0.5f) -> Mesh;
    auto create_sphere_mesh(float radius = 0.5f, uint32_t slices = 32, uint32_t stacks = 16) -> Mesh;
}
