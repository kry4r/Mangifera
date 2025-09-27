#include "model.hpp"

namespace mango::resource
{
    auto Mesh_Instance::get_mesh() const -> std::shared_ptr<Mesh> { return mesh; }

    auto Model::add_instance(std::shared_ptr<Mesh> mesh) -> Mesh_Instance&
    {
        instances.emplace_back(std::move(mesh));
        return instances.back();
    }

    auto Model::get_instances() -> std::vector<Mesh_Instance>& { return instances; }
    auto Model::get_instances() const -> const std::vector<Mesh_Instance>& { return instances; }

    auto Model::set_material(size_t idx, std::string mat) -> void
    {
        if (idx >= materials.size()) {
            materials.resize(idx + 1);
        }
        materials[idx] = std::move(mat);
    }

    auto Model::get_material(size_t idx) const -> const std::string&
    {
        static const std::string empty = "";
        return (idx < materials.size()) ? materials[idx] : empty;
    }
}
