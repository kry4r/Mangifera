#pragma once
#include "pipeline-state/compute-pipeline-state.hpp"
#include "vk-pipeline-layout.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace mango::graphics::vk
{
    class Vk_Compute_Pipeline_State : public Compute_Pipeline_State
    {
    public:
        Vk_Compute_Pipeline_State(VkDevice device, const Compute_Pipeline_Desc& desc);
        ~Vk_Compute_Pipeline_State();

        Vk_Compute_Pipeline_State(const Vk_Compute_Pipeline_State&) = delete;
        Vk_Compute_Pipeline_State& operator=(const Vk_Compute_Pipeline_State&) = delete;
        Vk_Compute_Pipeline_State(Vk_Compute_Pipeline_State&& other) noexcept;
        Vk_Compute_Pipeline_State& operator=(Vk_Compute_Pipeline_State&& other) noexcept;

        auto get_vk_pipeline() const -> VkPipeline { return m_pipeline; }
        auto get_vk_pipeline_layout() const -> VkPipelineLayout { return m_pipeline_layout->get_vk_pipeline_layout(); }

    private:
        void create_pipeline();
        void create_pipeline_layout();
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        std::unique_ptr<Vk_Pipeline_Layout> m_pipeline_layout;
    };

} // namespace mango::graphics::vk
