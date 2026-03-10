#include "render_core/render_graph.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

namespace mango::app
{
    void Render_Graph::add_pass(Render_Pass_Node node)
    {
        passes_.push_back(std::move(node));
    }

    auto Render_Graph::compile() const -> std::vector<std::string>
    {
        const std::size_t pass_count = passes_.size();
        std::vector<std::vector<std::size_t>> edges(pass_count);
        std::vector<std::size_t> indegree(pass_count, 0);
        std::unordered_map<std::string, std::size_t> producers;

        for (std::size_t index = 0; index < pass_count; ++index) {
            for (const auto& resource : passes_[index].writes) {
                producers[resource] = index;
            }
        }

        for (std::size_t index = 0; index < pass_count; ++index) {
            std::unordered_set<std::size_t> dependencies;
            for (const auto& resource : passes_[index].reads) {
                const auto it = producers.find(resource);
                if (it != producers.end() && it->second != index) {
                    dependencies.insert(it->second);
                }
            }

            for (const auto dependency : dependencies) {
                edges[dependency].push_back(index);
                ++indegree[index];
            }
        }

        std::vector<std::size_t> ready;
        ready.reserve(pass_count);
        for (std::size_t index = 0; index < pass_count; ++index) {
            if (indegree[index] == 0) {
                ready.push_back(index);
            }
        }

        std::vector<std::string> order;
        order.reserve(pass_count);

        while (!ready.empty()) {
            const auto current = ready.front();
            ready.erase(ready.begin());
            order.push_back(passes_[current].name);

            for (const auto dependent : edges[current]) {
                if (--indegree[dependent] == 0) {
                    ready.push_back(dependent);
                }
            }
        }

        if (order.size() != pass_count) {
            return {};
        }

        return order;
    }
}
