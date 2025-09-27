#pragma once
#include "core/data-struct/freelist.hpp"
#include "core/data-struct/scene-node.hpp"
#include "core/data-struct/singleton.hpp"
#include "core/base/gardener.hpp"
#include <unordered_map>
#include <memory>
#include <vector>

namespace mango::core
{

    struct Scene_Graph : core::Singleton<Scene_Graph>
    {
    private:
        Scene_Node root;
        Scene_Node current_selected_node;
        std::vector<std::pair<Entity,Scene_Node>> entities_mapping;
    public:

        Scene_Graph();

        auto get_root_node() -> Scene_Node;

        auto get_current_selected_node() -> Scene_Node;

        auto read_write_graph() -> Scene_Graph*;

        auto read_only_graph() -> const Scene_Graph*;

        auto path_of(Scene_Node node) -> std::string;

        auto node_of(std::string path) -> std::shared_ptr<Scene_Node>;

        auto name_of(Scene_Node node) -> std::string;

        auto parent_of(Scene_Node node) -> std::shared_ptr<Scene_Node>;

        auto first_child_of(Scene_Node node) -> std::shared_ptr<Scene_Node>;

        auto next_decendent_of(Scene_Node node) ->std::shared_ptr<Scene_Node>;
    };
}
