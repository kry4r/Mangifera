#pragma once
#include "base/twig.hpp"
#include "math/math.hpp"
#include "mesh.hpp"
#include <memory>
#include <vector>
#include <string>

namespace mango::resource
{
    struct Mesh_Instance
    {
        std::shared_ptr<Mesh> mesh;

        Mesh_Instance(std::shared_ptr<Mesh> m)
            : mesh(std::move(m)) {}

        auto get_mesh() const -> std::shared_ptr<Mesh>;
    };

    struct Model : core::Twig<Model>
    {
    private:
        std::vector<Mesh_Instance> instances;
        std::vector<std::string> materials;

    public:
        Model() = default;

        auto add_instance(std::shared_ptr<Mesh> mesh) -> Mesh_Instance&;

        auto get_instances() -> std::vector<Mesh_Instance>&;
        auto get_instances() const -> const std::vector<Mesh_Instance>&;

        auto set_material(size_t idx, std::string mat) -> void;

        auto get_material(size_t idx) const -> const std::string&;
    };
}
