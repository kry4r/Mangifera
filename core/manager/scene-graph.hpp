#pragma once
#include "data-struct/freelist.hpp"
#include "data-struct/scene-node.hpp"
#include "data-struct/singleton.hpp"
#include "base/gardener.hpp"
#include <unordered_map>
#include <memory>
#include <vector>

namespace mango::core
{

    struct Scene_Graph : core::Singleton<Scene_Graph>
    {
    private:
        std::shared_ptr<Scene_Node> root;
        std::shared_ptr<Scene_Node> current_selected_node;
        std::vector<std::pair<Entity, std::shared_ptr<Scene_Node>>> entities_mapping;
    public:

        Scene_Graph();

        auto get_root_node() -> std::shared_ptr<Scene_Node>;

        auto get_current_selected_node() -> std::shared_ptr<Scene_Node>;

        auto set_current_selected_node(std::shared_ptr<Scene_Node> node) -> void;

        auto read_write_graph() -> Scene_Graph*;

        auto read_only_graph() -> const Scene_Graph*;

        auto path_of(std::shared_ptr<Scene_Node> node) -> std::string;

        auto node_of(const std::string& path) -> std::shared_ptr<Scene_Node>;

        auto name_of(std::shared_ptr<Scene_Node> node) -> std::string;

        auto parent_of(std::shared_ptr<Scene_Node> node) -> std::shared_ptr<Scene_Node>;

        auto first_child_of(std::shared_ptr<Scene_Node> node) -> std::shared_ptr<Scene_Node>;

        auto next_decendent_of(std::shared_ptr<Scene_Node> node) -> std::shared_ptr<Scene_Node>;

        auto add_entity_to_scene(Entity entity, std::shared_ptr<Scene_Node> parent, const std::string& name = "") -> std::shared_ptr<Scene_Node>;

        auto get_entities_mapping() const -> const std::vector<std::pair<Entity, std::shared_ptr<Scene_Node>>>& { return entities_mapping; }
    };
}
