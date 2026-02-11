#include "model.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <tuple>

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

    namespace
    {
        struct ObjIndex
        {
            int v = 0;
            int vt = 0;
            int vn = 0;

            bool operator==(const ObjIndex& other) const
            {
                return v == other.v && vt == other.vt && vn == other.vn;
            }
        };

        struct ObjIndexHasher
        {
            std::size_t operator()(const ObjIndex& idx) const
            {
                std::size_t h1 = std::hash<int>{}(idx.v);
                std::size_t h2 = std::hash<int>{}(idx.vt);
                std::size_t h3 = std::hash<int>{}(idx.vn);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        auto parse_index(const std::string& token, int count) -> int
        {
            if (token.empty()) {
                return 0;
            }
            int idx = std::stoi(token);
            if (idx < 0) {
                idx = count + idx + 1;
            }
            return idx;
        }
    }

    auto load_model_from_obj(const std::string& path) -> std::shared_ptr<Model>
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return nullptr;
        }

        std::vector<math::Vec3> positions;
        std::vector<math::Vec3> normals;
        std::vector<math::Vec2> uvs;

        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::unordered_map<ObjIndex, std::uint32_t, ObjIndexHasher> index_map;

        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("v ", 0) == 0) {
                std::stringstream ss(line.substr(2));
                math::Vec3 pos{};
                ss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            } else if (line.rfind("vn ", 0) == 0) {
                std::stringstream ss(line.substr(3));
                math::Vec3 n{};
                ss >> n.x >> n.y >> n.z;
                normals.push_back(n);
            } else if (line.rfind("vt ", 0) == 0) {
                std::stringstream ss(line.substr(3));
                math::Vec2 uv{};
                ss >> uv.x >> uv.y;
                uvs.push_back(uv);
            } else if (line.rfind("f ", 0) == 0) {
                std::stringstream ss(line.substr(2));
                std::vector<ObjIndex> face_indices;
                std::string vert;
                while (ss >> vert) {
                    ObjIndex idx{};
                    size_t p1 = vert.find('/');
                    size_t p2 = vert.find('/', p1 == std::string::npos ? p1 : p1 + 1);

                    std::string v_str = p1 == std::string::npos ? vert : vert.substr(0, p1);
                    std::string vt_str;
                    std::string vn_str;

                    if (p1 != std::string::npos) {
                        if (p2 == std::string::npos) {
                            vt_str = vert.substr(p1 + 1);
                        } else {
                            vt_str = vert.substr(p1 + 1, p2 - p1 - 1);
                            vn_str = vert.substr(p2 + 1);
                        }
                    }

                    idx.v = parse_index(v_str, static_cast<int>(positions.size()));
                    idx.vt = parse_index(vt_str, static_cast<int>(uvs.size()));
                    idx.vn = parse_index(vn_str, static_cast<int>(normals.size()));
                    face_indices.push_back(idx);
                }

                if (face_indices.size() < 3) {
                    continue;
                }

                for (size_t i = 1; i + 1 < face_indices.size(); ++i) {
                    ObjIndex tri[3] = { face_indices[0], face_indices[i], face_indices[i + 1] };
                    for (const auto& idx : tri) {
                        auto it = index_map.find(idx);
                        if (it == index_map.end()) {
                            Vertex vtx{};
                            if (idx.v > 0 && idx.v <= static_cast<int>(positions.size())) {
                                vtx.position = positions[idx.v - 1];
                            }
                            if (idx.vn > 0 && idx.vn <= static_cast<int>(normals.size())) {
                                vtx.normal = normals[idx.vn - 1];
                            }
                            if (idx.vt > 0 && idx.vt <= static_cast<int>(uvs.size())) {
                                vtx.uv = uvs[idx.vt - 1];
                            }

                            std::uint32_t new_index = static_cast<std::uint32_t>(vertices.size());
                            vertices.push_back(vtx);
                            index_map[idx] = new_index;
                            indices.push_back(new_index);
                        } else {
                            indices.push_back(it->second);
                        }
                    }
                }
            }
        }

        if (vertices.empty()) {
            return nullptr;
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->set_vertices(vertices);
        mesh->set_indices(indices);

        auto model = std::make_shared<Model>();
        model->add_instance(mesh);
        return model;
    }
}
