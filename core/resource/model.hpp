#pragma once
#include "core/base/twig.hpp"
#include "core/math/math.hpp"
#include "mesh.hpp"
#include <memory>
#include <vector>
#include <string>

namespace mango::resource
{
    struct Mesh_Instance
    {
        std::shared_ptr<Mesh> mesh;

        Mesh_Instance(std::shared_ptr<Mesh> m, const math::Mat4& t)
            : mesh(std::move(m)) {}

        auto get_mesh() const -> std::shared_ptr<Mesh> { return mesh; }
    };

    struct Model : core::Twig<Model>
    {
    private:
        std::vector<Mesh_Instance> instances;
        std::vector<std::string> materials;

    public:
        Model() = default;

        auto add_instance(std::shared_ptr<Mesh> mesh) -> Mesh_Instance&
        {
            instances.emplace_back(std::move(mesh));
            return instances.back();
        }

        auto get_instances() -> std::vector<Mesh_Instance>& { return instances; }
        auto get_instances() const -> const std::vector<Mesh_Instance>& { return instances; }

        auto set_material(size_t idx, std::string mat) -> void
        {
            if (idx >= materials.size()) {
                materials.resize(idx + 1);
            }
            materials[idx] = std::move(mat);
        }

        auto get_material(size_t idx) const -> const std::string&
        {
            static const std::string empty = "";
            return (idx < materials.size()) ? materials[idx] : empty;
        }
    };
}
